#pragma once

#include "lvgl.h"
#include "model.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_services_create(void);
void      screen_services_update(lv_obj_t *screen, const status_model_t *m);

#ifdef __cplusplus
}
#endif
