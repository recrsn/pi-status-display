/*
 * HAL implementation for the desktop emulator.
 * Display: LVGL SDL2 driver (240x280 window)
 * Touch:   SDL2 mouse driver
 * Data:    Unix domain socket at SOCKET_PATH
 */

#ifndef ESP_PLATFORM

#include "hal.h"

#include <SDL2/SDL.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

#ifndef SOCKET_PATH
#define SOCKET_PATH "/tmp/pi-status.sock"
#endif

#ifndef DISPLAY_HOR_RES
#define DISPLAY_HOR_RES 240
#endif
#ifndef DISPLAY_VER_RES
#define DISPLAY_VER_RES 280
#endif

static hal_data_cb_t  g_data_cb;
static int            g_sock_fd = -1;
static pthread_t      g_reader_thread;
static lv_display_t  *g_display;
static lv_indev_t    *g_mouse;

/* --- Tick ----------------------------------------------------------------*/

uint32_t hal_tick_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

/* --- Socket reader (background thread) -----------------------------------*/

static void connect_socket(void) {
    struct sockaddr_un addr = {0};
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return; }

    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect " SOCKET_PATH);
        close(fd);
        return;
    }
    g_sock_fd = fd;
}

static void *reader_thread(void *arg) {
    (void)arg;
    char buf[2048];
    size_t used = 0;

    for (;;) {
        if (g_sock_fd < 0) {
            sleep(1);
            connect_socket();
            continue;
        }

        ssize_t n = read(g_sock_fd, buf + used, sizeof(buf) - used - 1);
        if (n <= 0) {
            close(g_sock_fd);
            g_sock_fd = -1;
            used = 0;
            continue;
        }

        used += (size_t)n;
        buf[used] = '\0';

        char *start = buf;
        char *nl;
        while ((nl = (char *)memchr(start, '\n', (size_t)(buf + used - start))) != NULL) {
            *nl = '\0';
            if (g_data_cb) g_data_cb(start, (size_t)(nl - start));
            start = nl + 1;
        }

        /* Shift remaining partial line to front */
        used = (size_t)(buf + used - start);
        memmove(buf, start, used);
    }
    return NULL;
}

/* --- Command sender ------------------------------------------------------*/

void hal_send_command(const char *cmd_json) {
    if (g_sock_fd < 0) return;
    size_t len = strlen(cmd_json);
    write(g_sock_fd, cmd_json, len);
    write(g_sock_fd, "\n", 1);
}

/* --- Screenshot (docs) -----------------------------------------------------
 * Press 's' to dump the active screen to a raw PPM, pixel-for-pixel as LVGL
 * rendered it — no window chrome, no macOS screencapture/Retina scaling.
 * Post-process with tools/snapshot.py (PPM -> PNG, downscaled 2x). */
#if LV_USE_SNAPSHOT

#define SNAPSHOT_PATH "/tmp/pi-status-snapshot.ppm"

static void save_snapshot(void) {
    lv_draw_buf_t *buf = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);
    if (!buf) {
        fprintf(stderr, "snapshot: lv_snapshot_take failed\n");
        return;
    }

    FILE *f = fopen(SNAPSHOT_PATH, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", (int)buf->header.w, (int)buf->header.h);
        /* lv_color_t (what LV_COLOR_FORMAT_RGB888 buffers use) is laid out
         * {blue, green, red} in memory, not PPM's required R,G,B — swap
         * per pixel rather than assuming the format name is the byte order. */
        for (int32_t y = 0; y < buf->header.h; y++) {
            uint8_t *row = buf->data + (size_t)y * buf->header.stride;
            for (int32_t x = 0; x < buf->header.w; x++) {
                uint8_t rgb[3] = { row[x * 3 + 2], row[x * 3 + 1], row[x * 3 + 0] };
                fwrite(rgb, 1, 3, f);
            }
        }
        fclose(f);
        fprintf(stderr, "snapshot: wrote %s\n", SNAPSHOT_PATH);
    }
    lv_draw_buf_destroy(buf);
}

static int snapshot_event_watch(void *userdata, SDL_Event *event) {
    (void)userdata;
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_s) {
        save_snapshot();
    }
    return 1;
}
#endif /* LV_USE_SNAPSHOT */

/* --- Init ----------------------------------------------------------------*/

void hal_init(hal_data_cb_t on_data) {
    g_data_cb = on_data;

    lv_init();

    /* SDL display via LVGL's built-in driver */
    g_display = lv_sdl_window_create(DISPLAY_HOR_RES, DISPLAY_VER_RES);
    lv_display_set_default(g_display);

    g_mouse = lv_sdl_mouse_create();
    lv_indev_set_display(g_mouse, g_display);

#if LV_USE_SNAPSHOT
    SDL_AddEventWatch(snapshot_event_watch, NULL);
#endif

    /* Background socket reader */
    pthread_create(&g_reader_thread, NULL, reader_thread, NULL);
}

#endif // !ESP_PLATFORM
