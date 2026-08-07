#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Implemented by hal_esp32.c (firmware) and hal_sdl.c (emulator). */

/* Called on each received newline-delimited JSON packet. */
typedef void (*hal_data_cb_t)(const char *json, size_t len);

/* Initialise display, touch, and data transport. */
void hal_init(hal_data_cb_t on_data);

/* Send a fire-and-forget command JSON line to the Pi agent. */
void hal_send_command(const char *cmd_json);

/* Print a raw (non-JSON) diagnostic line. On firmware this shares the
 * data transport, so the Pi agent picks it up as board console output;
 * on the emulator it goes to stderr. */
void hal_debug_print(const char *line);

/* Return monotonic milliseconds for timeout detection (not the LVGL tick).
 * SDL driver registers lv_tick_set_cb(SDL_GetTicks) automatically;
 * firmware hal_init calls lv_tick_set_cb(hal_tick_ms). */
uint32_t hal_tick_ms(void);

#ifdef __cplusplus
}
#endif
