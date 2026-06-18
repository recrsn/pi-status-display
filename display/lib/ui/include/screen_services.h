#pragma once

#include "lvgl.h"
#include "model.h"

lv_obj_t *screen_services_create(void);
void      screen_services_update(lv_obj_t *screen, const status_model_t *m);
