/*
 * HAL implementation for ESP32-S3 hardware — Waveshare ESP32-S3-Touch-LCD-1.69.
 * Display: ST7789V2 240x280 via SPI (esp_lcd)
 * Touch:   CST816S via I2C (esp_lcd_touch_cst816s)
 * Data:    the board's USB-Serial-JTAG peripheral (already the boot/flash
 *          port, appears as /dev/cu.usbmodemXXX with no extra USB stack
 *          needed), talked to directly via usb_serial_jtag_read/write_bytes.
 *          Not routed through stdio/console: doing that (fgets/printf over
 *          the console's VFS) repeatedly deadlocked or asserted against the
 *          driver's own ISR under load — two independent consumers of the
 *          same hardware. Console is disabled entirely (sdkconfig.defaults).
 *
 * Pin assignments come from Waveshare's official examples for this exact
 * board (github.com/waveshareteam/ESP32-S3-Touch-LCD-1.69), not guessed.
 */

#ifdef ESP_PLATFORM

#include "hal.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include <assert.h>
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

/* ---- Pin map (Waveshare ESP32-S3-Touch-LCD-1.69) -----------------------*/
#define PIN_LCD_SCLK   GPIO_NUM_6
#define PIN_LCD_MOSI   GPIO_NUM_7
#define PIN_LCD_CS     GPIO_NUM_5
#define PIN_LCD_DC     GPIO_NUM_4
#define PIN_LCD_RST    GPIO_NUM_8
#define PIN_LCD_BL     GPIO_NUM_15
#define PIN_TOUCH_SDA  GPIO_NUM_11
#define PIN_TOUCH_SCL  GPIO_NUM_10
#define PIN_TOUCH_INT  GPIO_NUM_14
#define PIN_TOUCH_RST  GPIO_NUM_13

#define LCD_HOR_RES  240
#define LCD_VER_RES  280
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8
#define LCD_SPI_HOST SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ (80 * 1000 * 1000)

#define TOUCH_I2C_PORT I2C_NUM_0

/* Guards usb_serial_jtag_write_bytes: parsing now runs on the RX task
 * (core 1) and can call hal_debug_print concurrently with the LVGL task's
 * (core 0) own debug/command writes. Without this, interleaved writes from
 * both cores would garble each other's lines on the wire. */
static SemaphoreHandle_t g_tx_mutex;

static hal_data_cb_t  g_data_cb;
static lv_display_t  *g_display;
static lv_indev_t    *g_touch_indev;
static esp_lcd_panel_handle_t g_panel;
static esp_lcd_touch_handle_t g_touch;

/* ---- Tick ---------------------------------------------------------------*/

uint32_t hal_tick_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* ---- Display flush callback --------------------------------------------*/

static bool on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                 esp_lcd_panel_io_event_data_t *edata,
                                 void *user_ctx) {
    lv_display_flush_ready(g_display);
    return false;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    /* In LV_DISPLAY_RENDER_MODE_DIRECT, layer->buf_area is always the whole
     * screen (lv_refr.c) — px_map is the constant full-buffer base pointer,
     * not offset/strided to `area`, on every call. esp_lcd_panel_draw_bitmap
     * has no stride parameter, so honoring `area`'s x-range as-is (narrower
     * than the buffer's real row width) feeds it misaligned rows past the
     * first. Extending to the full row width keeps addressing correct —
     * full-width rows are contiguous in a row-major buffer, so this is a
     * valid tightly-packed span sized to just the dirty rows, not the
     * whole frame. */
    uint16_t *buf = (uint16_t *)px_map;

    /* Extending to the full row width for the transfer itself keeps
     * addressing correct regardless of swap range — full-width rows are
     * contiguous in a row-major buffer, satisfying esp_lcd_panel_draw_bitmap's
     * no-stride requirement, sized to just the dirty rows, not the whole
     * frame. See the earlier comment history in git blame for the
     * full diagnosis (px_map is always the whole-buffer base pointer in
     * LV_DISPLAY_RENDER_MODE_DIRECT, not offset to `area`). */
    uint16_t *row_start = buf + (size_t)area->y1 * LCD_HOR_RES;
    esp_lcd_panel_draw_bitmap(g_panel, 0, area->y1, LCD_HOR_RES, area->y2 + 1, row_start);
}

/* ---- Touch read callback -------------------------------------------------*/

/* CST816T only keeps its I2C interface live around a touch event, so most
 * polls fail — that's tolerated silently (falls through to RELEASED) rather
 * than treated as an error. An interrupt-driven version was tried, but the
 * INT line free-runs on noise and firing a GPIO ISR at that rate starves the
 * idle task hard enough to trip the watchdog; simple polling is stable. */
static void lvgl_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    uint16_t x, y;
    uint8_t cnt = 0;

    esp_lcd_touch_read_data(g_touch);
    bool pressed = esp_lcd_touch_get_coordinates(g_touch, &x, &y, NULL, &cnt, 1) && cnt > 0;

    if (pressed) {
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

/* ---- USB-Serial-JTAG data reception --------------------------------------*/

/* Talks to the driver directly (usb_serial_jtag_read_bytes), not through
 * stdio/fgets — see sdkconfig.defaults for why. We do our own line
 * assembly: read whatever bytes are available, split on '\n', carry any
 * partial trailing line over to the next read. */
static void usb_rx_task(void *arg) {
    (void)arg;
    static char line[2048];
    size_t line_len = 0;
    uint8_t chunk[256];

    for (;;) {
        int n = usb_serial_jtag_read_bytes(chunk, sizeof(chunk), portMAX_DELAY);
        if (n <= 0) continue;

        for (int i = 0; i < n; i++) {
            char c = (char)chunk[i];
            if (c == '\n') {
                while (line_len > 0 && line[line_len - 1] == '\r') line_len--;
                if (g_data_cb && line_len > 0) g_data_cb(line, line_len);
                line_len = 0;
            } else if (line_len < sizeof(line) - 1) {
                line[line_len++] = c;
            } else {
                /* Line too long for the buffer: drop it and resync on the
                 * next '\n' rather than silently truncating/misframing. */
                line_len = 0;
            }
        }
    }
}

/* ---- Command sender ----------------------------------------------------*/

void hal_send_command(const char *cmd_json) {
    xSemaphoreTake(g_tx_mutex, portMAX_DELAY);
    size_t len = strlen(cmd_json);
    usb_serial_jtag_write_bytes(cmd_json, len, portMAX_DELAY);
    const char nl = '\n';
    usb_serial_jtag_write_bytes(&nl, 1, portMAX_DELAY);
    xSemaphoreGive(g_tx_mutex);
}

void hal_debug_print(const char *line) {
    xSemaphoreTake(g_tx_mutex, portMAX_DELAY);
    usb_serial_jtag_write_bytes(line, strlen(line), portMAX_DELAY);
    const char nl = '\n';
    usb_serial_jtag_write_bytes(&nl, 1, portMAX_DELAY);
    xSemaphoreGive(g_tx_mutex);
}

/* ---- Init helpers --------------------------------------------------------*/

static void init_lcd(void) {
    gpio_config_t bk_gpio_config = {
        .pin_bit_mask = 1ULL << PIN_LCD_BL,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&bk_gpio_config);

    spi_bus_config_t buscfg = {
        .mosi_io_num   = PIN_LCD_MOSI,
        .miso_io_num   = -1,
        .sclk_io_num   = PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        /* Must cover the largest single flush, which since DIRECT mode
         * moved to a full-frame draw buffer can be up to the whole
         * screen. Undersized here means the SPI driver silently splits
         * one logical flush into multiple hardware transactions, each
         * firing its own completion callback -> lv_display_flush_ready()
         * called more times than LVGL expects -> buffer-swap bookkeeping
         * desyncs over time (visible as garbage after a few seconds). */
        .max_transfer_sz = LCD_HOR_RES * LCD_VER_RES * sizeof(uint16_t),
    };
    spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);

    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num         = PIN_LCD_CS,
        .dc_gpio_num         = PIN_LCD_DC,
        .spi_mode            = 0,
        .pclk_hz             = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth   = 10,
        .on_color_trans_done = on_color_trans_done,
        .user_ctx            = NULL,
        .lcd_cmd_bits        = LCD_CMD_BITS,
        .lcd_param_bits      = LCD_PARAM_BITS,
    };
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &io_handle);

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .bits_per_pixel = 16,
    };
    esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &g_panel);
    esp_lcd_panel_reset(g_panel);
    esp_lcd_panel_init(g_panel);

    /* Panel-specific quirks for this exact module (Waveshare 1.69"):
     * mirrored in both axes, 20px vertical gap (driver IC framebuffer is
     * taller than the visible 280px), and inverted color. */
    esp_lcd_panel_mirror(g_panel, false, false);
    esp_lcd_panel_set_gap(g_panel, 0, 20);
    esp_lcd_panel_invert_color(g_panel, true);
    esp_lcd_panel_disp_on_off(g_panel, true);
    gpio_set_level(PIN_LCD_BL, 1);

    g_display = lv_display_create(LCD_HOR_RES, LCD_VER_RES);
    lv_display_set_flush_cb(g_display, lvgl_flush_cb);
    /* ST7789 wants big-endian RGB565 over SPI; LVGL's software renderer
     * writes swapped bytes natively when told the buffer is in this
     * format, so no flush-time byte swap or panel RAMCTRL override is
     * needed. */
    lv_display_set_color_format(g_display, LV_COLOR_FORMAT_RGB565_SWAPPED);

    /* Full-frame buffer instead of a 40-line rolling buffer. The
     * frame-time counter showed the old PARTIAL-mode cost wasn't SPI bit
     * time — it was ~3.7ms fixed dispatch overhead paid per flush call,
     * ~18 times per data update (one per scattered small widget), not
     * per byte. DIRECT mode renders every dirty area into this one
     * screen-sized buffer and flushes far fewer, larger regions (measured:
     * 70ms/18 flushes -> 25ms/5 flushes).
     *
     * Tried this buffer in PSRAM (8MB Octal, confirmed present on this
     * module) to avoid the internal-SRAM cost, with MALLOC_CAP_DMA and
     * 64-byte cache-line alignment to keep the SPI driver's per-transfer
     * cache writeback correct. Still produced visible corruption, worse
     * than before those fixes — evidently still missing something about
     * making DMA-from-PSRAM cache-coherent here. Internal SRAM has no
     * such coherency hazard (not behind the same cached/external-bus
     * path) and is what ran clean for this whole project before today;
     * 134KB fits comfortably in the 320KB budget. Revisit PSRAM only
     * with a from-scratch root-cause, not another targeted patch. */
    size_t draw_buf_size = LCD_HOR_RES * LCD_VER_RES * sizeof(uint16_t);
    uint16_t *draw_buf = heap_caps_malloc(draw_buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    assert(draw_buf);
    lv_display_set_buffers(g_display, draw_buf, NULL, draw_buf_size,
                           LV_DISPLAY_RENDER_MODE_DIRECT);
}

static void init_touch(void) {
    i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_TOUCH_SDA,
        .scl_io_num = PIN_TOUCH_SCL,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = { .clk_speed = 100 * 1000 },
    };
    i2c_param_config(TOUCH_I2C_PORT, &i2c_cfg);
    i2c_driver_install(TOUCH_I2C_PORT, i2c_cfg.mode, 0, 0, 0);

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    tp_io_cfg.scl_speed_hz = 0; /* new-driver-only field; legacy i2c backend rejects it if set */
    esp_lcd_new_panel_io_i2c((esp_lcd_i2c_bus_handle_t)TOUCH_I2C_PORT, &tp_io_cfg, &tp_io_handle);

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_HOR_RES,
        .y_max = LCD_VER_RES,
        .rst_gpio_num = PIN_TOUCH_RST,
        .int_gpio_num = PIN_TOUCH_INT,
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 0 },
    };
    esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &g_touch);

    /* This is actually a CST816T, not the CST816S the Espressif driver
     * targets: the T variant auto-sleeps its I2C interface between reports
     * (only reachable right after reset, which is why the chip-id read in
     * esp_lcd_touch_new_i2c_cst816s succeeds but later polls don't). Write
     * REG_DIS_AUTOSLEEP=0xFF to keep it awake, per
     * https://github.com/koendv/cst816t/blob/master/src/cst816t.cpp */
    uint8_t dis_autosleep = 0xFF;
    esp_lcd_panel_io_tx_param(tp_io_handle, 0xFE, &dis_autosleep, 1);

    g_touch_indev = lv_indev_create();
    lv_indev_set_type(g_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(g_touch_indev, g_display);
    lv_indev_set_read_cb(g_touch_indev, lvgl_touch_read_cb);
}

/* ---- Init --------------------------------------------------------------*/

void hal_init(hal_data_cb_t on_data) {
    g_data_cb = on_data;
    g_tx_mutex = xSemaphoreCreateMutex();
    assert(g_tx_mutex);

    /* Console is disabled (sdkconfig.defaults); we own USB-Serial-JTAG
     * outright via the low-level driver, no VFS/stdio involved. */
    usb_serial_jtag_driver_config_t usb_cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    usb_serial_jtag_driver_install(&usb_cfg);

    /* Pinned to core 1: app_main (LVGL render/flush, CONFIG_ESP_MAIN_TASK_AFFINITY)
     * runs on core 0. Left unpinned, the scheduler is free to place this
     * higher-priority task (5 vs main's 1) on core 0 too, where it can
     * preempt a render pass exactly when new serial data arrives — the
     * frame-time counter caught spikes lining up with parse/update calls.
     * ui_on_data (called from here) now also does the JSON parse itself
     * (cJSON), so this core-1/core-0 split keeps parsing fully off the
     * render+DMA core, not just line framing. Stack bumped from the prior
     * 4096 to cover cJSON's parse-time recursion plus a status_model_t
     * struct copy (~1.8KB) that framing-only code never touched. */
    xTaskCreatePinnedToCore(usb_rx_task, "usb_rx", 8192, NULL, 5, NULL, 1);

    lv_init();
    lv_tick_set_cb(hal_tick_ms);

    init_lcd();
    init_touch();
}

#endif // ESP_PLATFORM
