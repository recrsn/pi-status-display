/*
 * HAL implementation for ESP32-S3 hardware.
 * Display: ST7789V2 240x280 via SPI (esp_lcd)
 * Touch:   CST816 via I2C
 * Data:    USB CDC (tinyusb / esp_tinyusb)
 *
 * Pin assignments — adjust to match your board wiring.
 */

#include "hal.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include <string.h>
#include <stdio.h>

/* ---- Pin map (edit per board) ----------------------------------------*/
#define PIN_LCD_SCLK   GPIO_NUM_12
#define PIN_LCD_MOSI   GPIO_NUM_11
#define PIN_LCD_CS     GPIO_NUM_10
#define PIN_LCD_DC     GPIO_NUM_9
#define PIN_LCD_RST    GPIO_NUM_8
#define PIN_LCD_BL     GPIO_NUM_46
#define PIN_TOUCH_SDA  GPIO_NUM_5
#define PIN_TOUCH_SCL  GPIO_NUM_6
#define PIN_TOUCH_INT  GPIO_NUM_7

#define LCD_HOR_RES  240
#define LCD_VER_RES  280
#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8
#define LCD_SPI_HOST SPI2_HOST
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)

static hal_data_cb_t  g_data_cb;
static lv_display_t  *g_display;
static lv_indev_t    *g_touch;
static esp_lcd_panel_handle_t g_panel;

/* ---- Tick ---------------------------------------------------------------*/

uint32_t hal_tick_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* Single LVGL task on ESP32 — no concurrent LVGL access, no-ops needed. */
void hal_lock(void)   {}
void hal_unlock(void) {}

/* ---- Display flush callback --------------------------------------------*/

static bool on_color_trans_done(esp_lcd_panel_io_handle_t io,
                                 esp_lcd_panel_io_event_data_t *edata,
                                 void *user_ctx) {
    lv_display_flush_ready(g_display);
    return false;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    esp_lcd_panel_draw_bitmap(g_panel,
        area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
}

/* ---- USB CDC data reception --------------------------------------------*/

static char   g_rx_buf[1024];
static size_t g_rx_used = 0;

void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event) {
    (void)itf;
    uint8_t chunk[256];
    size_t rx_size = 0;

    esp_err_t ret = tinyusb_cdcacm_read(itf, chunk, sizeof(chunk), &rx_size);
    if (ret != ESP_OK || rx_size == 0) return;

    size_t avail = sizeof(g_rx_buf) - g_rx_used - 1;
    if (rx_size > avail) rx_size = avail;
    memcpy(g_rx_buf + g_rx_used, chunk, rx_size);
    g_rx_used += rx_size;
    g_rx_buf[g_rx_used] = '\0';

    char *start = g_rx_buf;
    char *nl;
    while ((nl = (char *)memchr(start, '\n', (size_t)(g_rx_buf + g_rx_used - start))) != NULL) {
        *nl = '\0';
        if (g_data_cb) g_data_cb(start, (size_t)(nl - start));
        start = nl + 1;
    }
    g_rx_used = (size_t)(g_rx_buf + g_rx_used - start);
    memmove(g_rx_buf, start, g_rx_used);
}

/* ---- Command sender ----------------------------------------------------*/

void hal_send_command(const char *cmd_json) {
    tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, (const uint8_t *)cmd_json, strlen(cmd_json));
    tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, (const uint8_t *)"\n", 1);
    tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
}

/* ---- Init --------------------------------------------------------------*/

void hal_init(hal_data_cb_t on_data) {
    g_data_cb = on_data;

    /* SPI bus */
    spi_bus_config_t buscfg = {
        .mosi_io_num   = PIN_LCD_MOSI,
        .miso_io_num   = -1,
        .sclk_io_num   = PIN_LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    spi_bus_initialize(LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);

    /* LCD panel IO */
    esp_lcd_panel_io_handle_t io_handle;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .cs_gpio_num         = PIN_LCD_CS,
        .dc_gpio_num         = PIN_LCD_DC,
        .spi_clock_hz        = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits        = LCD_CMD_BITS,
        .lcd_param_bits      = LCD_PARAM_BITS,
        .on_color_trans_done = on_color_trans_done,
        .user_ctx            = NULL,
        .flags.sio_mode      = 0,
    };
    esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &io_handle);

    /* ST7789 panel */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &g_panel);
    esp_lcd_panel_reset(g_panel);
    esp_lcd_panel_init(g_panel);
    esp_lcd_panel_set_gap(g_panel, 0, 0);
    esp_lcd_panel_disp_on_off(g_panel, true);
    gpio_set_level(PIN_LCD_BL, 1);

    /* LVGL display */
    g_display = lv_display_create(LCD_HOR_RES, LCD_VER_RES);
    lv_display_set_flush_cb(g_display, lvgl_flush_cb);
    static uint16_t draw_buf[LCD_HOR_RES * 40];
    lv_display_set_buffers(g_display, draw_buf, NULL, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* USB CDC */
    tinyusb_config_t tusb_cfg = {};
    tinyusb_driver_install(&tusb_cfg);
    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev = TINYUSB_USBDEV_0,
        .cdc_port = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 512,
        .callback_rx = &tinyusb_cdc_rx_callback,
    };
    tusb_cdc_acm_init(&acm_cfg);

    /* TODO: initialise CST816 touch via I2C and register lv_indev */
}
