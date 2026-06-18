#pragma once

#include "lvgl.h"
#include "model.h"

lv_obj_t *screen_overview_create(void);
void      screen_overview_update(lv_obj_t *screen, const status_model_t *m);
