#pragma once

#include "lvgl.h"
#include "model.h"

lv_obj_t *screen_network_create(void);
void screen_network_update(lv_obj_t *scr, const status_model_t *m);
