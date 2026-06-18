#pragma once

#include "lvgl.h"

/* Implemented by hal_esp32.cpp (firmware) and hal_sdl.cpp (emulator). */

/* Called on each received newline-delimited JSON packet. */
typedef void (*hal_data_cb_t)(const char *json, size_t len);

/* Initialise display, touch, and data transport. */
void hal_init(hal_data_cb_t on_data);

/* Send a fire-and-forget command JSON line to the Pi agent. */
void hal_send_command(const char *cmd_json);

/* Return monotonic milliseconds for timeout detection (not the LVGL tick).
 * SDL driver registers lv_tick_set_cb(SDL_GetTicks) automatically;
 * firmware hal_init calls lv_tick_set_cb(hal_tick_ms). */
uint32_t hal_tick_ms(void);

/* LVGL is not thread-safe. Call hal_lock/hal_unlock around any LVGL call made
 * from a non-main thread, and around lv_timer_handler() on the main thread.
 * Firmware implementation is a no-op (single LVGL task). */
void hal_lock(void);
void hal_unlock(void);
