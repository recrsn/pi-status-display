#include "statusbar.h"
#include "fonts.h"
#include "icons.h"
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <stdio.h>

typedef struct {
    lv_obj_t *icon;
    lv_obj_t *hostname;
    lv_obj_t *dot;
    lv_obj_t *time;
} statusbar_widgets_t;

static statusbar_widgets_t g_w;

void statusbar_create(void) {
    lv_obj_t *bar = lv_obj_create(lv_layer_top());
    lv_obj_set_size(bar, LV_PCT(100), 28);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_hor(bar, 12, 0);
    lv_obj_set_style_pad_ver(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *left = lv_obj_create(bar);
    lv_obj_remove_style_all(left);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(left, 5, 0);
    lv_obj_set_flex_grow(left, 1);

    g_w.icon = lv_image_create(left);
    lv_image_set_src(g_w.icon, &icon_server);
    lv_obj_set_style_image_recolor(g_w.icon, lv_color_hex(0xc8e0f0), 0);
    lv_obj_set_style_image_recolor_opa(g_w.icon, LV_OPA_COVER, 0);

    g_w.hostname = lv_label_create(left);
    lv_label_set_text(g_w.hostname, "");
    lv_obj_set_style_text_font(g_w.hostname, &jbmono_10, 0);
    lv_obj_set_style_text_color(g_w.hostname, lv_color_hex(0xc8e0f0), 0);
    lv_obj_set_style_text_letter_space(g_w.hostname, 1, 0);
    lv_label_set_long_mode(g_w.hostname, LV_LABEL_LONG_CLIP);
    lv_obj_set_flex_grow(g_w.hostname, 1);

    lv_obj_t *right = lv_obj_create(bar);
    lv_obj_remove_style_all(right);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 5, 0);

    g_w.dot = lv_obj_create(right);
    lv_obj_remove_style_all(g_w.dot);
    lv_obj_set_size(g_w.dot, 5, 5);
    lv_obj_set_style_radius(g_w.dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(g_w.dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_w.dot, lv_color_hex(0xf87070), 0);

    g_w.time = lv_label_create(right);
    lv_label_set_text(g_w.time, "--:--");
    lv_obj_set_style_text_font(g_w.time, &jbmono_12, 0);
    lv_obj_set_style_text_color(g_w.time, lv_color_hex(0x8cb4cc), 0);
}

void statusbar_update(const status_model_t *m) {
    char host_upper[MODEL_MAX_HOSTNAME];
    size_t hlen = strlen(m->hostname);
    if (hlen >= sizeof(host_upper)) hlen = sizeof(host_upper) - 1;
    for (size_t i = 0; i < hlen; i++) host_upper[i] = (char)toupper((unsigned char)m->hostname[i]);
    host_upper[hlen] = '\0';
    lv_label_set_text(g_w.hostname, host_upper);

    lv_obj_set_style_bg_color(g_w.dot,
        m->valid ? lv_color_hex(0x34d498) : lv_color_hex(0xf87070), 0);

    char buf[8];
    if (m->ts > 0) {
        time_t t = (time_t)m->ts;
        struct tm *tm_info = localtime(&t);
        strftime(buf, sizeof(buf), "%H:%M", tm_info);
    } else {
        snprintf(buf, sizeof(buf), "--:--");
    }
    lv_label_set_text(g_w.time, buf);
}
