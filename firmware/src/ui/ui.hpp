#pragma once

#include "lvgl.h"
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Initialise LVGL and create all screens. Call after hal_init(). */
void ui_init(void);

/* Feed a received JSON packet into the model and refresh the active screen. */
void ui_on_data(const char *json, size_t len);

/* Advance animations; call every ~5 ms from the main loop. */
void ui_tick(void);

#ifdef __cplusplus
}
#endif
