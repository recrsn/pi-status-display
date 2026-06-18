#include "ui.h"
#include "hal.h"
#include "screen_overview.h"
#include "screen_services.h"
#include <string.h>

#define DATA_TIMEOUT_MS 3000

static lv_obj_t *scr_overview;
static lv_obj_t *scr_services;
static lv_obj_t *scr_connecting;

static lv_obj_t *make_connecting_screen(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Connecting...");
    lv_obj_center(label);
    return scr;
}

void ui_init(void) {
    scr_connecting = make_connecting_screen();
    scr_overview   = screen_overview_create();
    scr_services   = screen_services_create();

    lv_screen_load(scr_connecting);
}

void ui_on_data(const char *json, size_t len) {
    status_model_t *m = model_get();
    if (!model_parse(m, json, len)) return;

    m->last_update_ms = hal_tick_ms();

    if (lv_screen_active() == scr_connecting) {
        lv_screen_load_anim(scr_overview, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }

    screen_overview_update(scr_overview, m);
    screen_services_update(scr_services, m);
}

void ui_tick(void) {
    status_model_t *m = model_get();
    hal_lock();
    if (m->valid && (hal_tick_ms() - m->last_update_ms) > DATA_TIMEOUT_MS) {
        if (lv_screen_active() != scr_connecting) {
            lv_screen_load(scr_connecting);
        }
    }
    lv_timer_handler();
    hal_unlock();
}
