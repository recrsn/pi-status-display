#include "screen_services.h"
#include "fonts.h"
#include <stdio.h>
#include <string.h>

/* Layout: 240 x 280, status bar (28px) overlays the top.
 *
 *  ┌────────────────────────────┐
 *  │ SERVICES              N/M  │
 *  │                            │
 *  │ o nginx                    │
 *  │ o sshd                     │
 *  │ .  postgres                │
 *  │ o docker                   │
 *  │  ...                       │
 *  └────────────────────────────┘
 */

typedef struct {
    lv_obj_t *count_label;
    lv_obj_t *list;
    lv_obj_t *rows[MODEL_MAX_SERVICES];
    lv_obj_t *dots[MODEL_MAX_SERVICES];
    lv_obj_t *names[MODEL_MAX_SERVICES];
} services_widgets_t;

static services_widgets_t *get_widgets(lv_obj_t *scr) {
    return (services_widgets_t *)lv_obj_get_user_data(scr);
}

lv_obj_t *screen_services_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_pad_hor(scr, 12, 0);
    lv_obj_set_style_pad_top(scr, 32, 0);
    lv_obj_set_style_pad_bottom(scr, 8, 0);
    lv_obj_set_style_pad_row(scr, 0, 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    services_widgets_t *w = static_cast<services_widgets_t *>(lv_malloc(sizeof(services_widgets_t)));
    lv_obj_set_user_data(scr, w);

    /* Header row: "SERVICES" left, "N/M" right */
    lv_obj_t *header = lv_obj_create(scr);
    lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(header, 4, 0);
    lv_obj_set_style_pad_hor(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "SERVICES");
    lv_obj_set_style_text_font(title, &jbmono_10, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x3c5878), 0);
    lv_obj_set_style_text_letter_space(title, 1, 0);

    w->count_label = lv_label_create(header);
    lv_label_set_text(w->count_label, "-/-");
    lv_obj_set_style_text_font(w->count_label, &jbmono_12, 0);
    lv_obj_set_style_text_color(w->count_label, lv_color_hex(0x24d4ec), 0);

    /* List container. Rows are pre-created once below (fixed pool sized to
     * MODEL_MAX_SERVICES) and shown/hidden per update instead of being
     * destroyed and rebuilt — lv_obj_clean() + recreate was paying a full
     * nested-flex layout pass every tick regardless of whether the service
     * list actually changed. */
    w->list = lv_obj_create(scr);
    lv_obj_set_size(w->list, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(w->list, 0, 0);
    lv_obj_set_style_border_width(w->list, 0, 0);
    lv_obj_set_style_bg_opa(w->list, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(w->list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(w->list, LV_FLEX_FLOW_COLUMN);

    for (int i = 0; i < MODEL_MAX_SERVICES; i++) {
        lv_obj_t *row = lv_obj_create(w->list);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_ver(row, 7, 0);
        lv_obj_set_style_pad_hor(row, 0, 0);
        lv_obj_set_style_pad_column(row, 8, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
        w->rows[i] = row;

        lv_obj_t *dot = lv_obj_create(row);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 6, 6);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        w->dots[i] = dot;

        lv_obj_t *name = lv_label_create(row);
        lv_obj_set_style_text_font(name, &jbmono_12, 0);
        lv_obj_set_style_text_color(name, lv_color_hex(0xc8e0f0), 0);
        lv_obj_set_flex_grow(name, 1);
        lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);
        w->names[i] = name;
    }

    return scr;
}

void screen_services_update(lv_obj_t *scr, const status_model_t *m) {
    services_widgets_t *w = get_widgets(scr);
    char buf[32];

    int active_count = 0;
    for (int i = 0; i < m->service_count; i++) {
        if (m->services[i].active) active_count++;
    }
    snprintf(buf, sizeof(buf), "%d/%d", active_count, m->service_count);
    lv_label_set_text(w->count_label, buf);
    lv_obj_set_style_text_color(w->count_label,
        active_count == m->service_count ? lv_color_hex(0x34d498) : lv_color_hex(0xf4a00c), 0);

    for (int i = 0; i < MODEL_MAX_SERVICES; i++) {
        if (i >= m->service_count) {
            lv_obj_add_flag(w->rows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }

        const service_t *s = &m->services[i];
        lv_obj_clear_flag(w->rows[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(w->dots[i],
            s->active ? lv_color_hex(0x34d498) : lv_color_hex(0x1c2434), 0);
        lv_label_set_text(w->names[i], s->name);
    }
}
