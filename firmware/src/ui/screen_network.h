#pragma once

#include "lvgl.h"
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_network_create(void);
void screen_network_update(lv_obj_t *scr, const status_model_t *m);

#ifdef __cplusplus
}
#endif
