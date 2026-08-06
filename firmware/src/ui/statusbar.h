#pragma once

#include "lvgl.h"
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Persistent bar on lv_layer_top(): hostname + online dot + clock.
 * Shown above every content screen, matching the reference design where
 * the status bar lives outside the swipeable page area. */
void statusbar_create(void);
void statusbar_update(const status_model_t *m);

#ifdef __cplusplus
}
#endif
