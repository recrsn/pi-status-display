#include "screen_overview.h"
#include "fonts.h"
#include "icons.h"
#include "hal.h"
#include <stdio.h>
#include <string.h>

/* Layout: 240 wide x 280 tall (portrait), status bar (28px) drawn separately
 * on lv_layer_top() covers the space above this screen's content.
 *
 *  ┌────────────────────────────┐
 *  │ (status bar, overlay)      │
 *  │ 192.168.1.42               │
 *  │                            │
 *  │  [cpu] [mem] [disk] [temp] │
 *  │  45%   860M   23%    48°   │
 *  │  ──    ───    ─      ──    │  color: cyan/amber/red by threshold
 *  │                            │
 *  │ ^ 0 B/s          1.0 KB/s v│
 *  └────────────────────────────┘
 */

typedef struct {
    lv_obj_t *ip;
    lv_obj_t *cpu_icon, *cpu_bar, *cpu_val;
    lv_obj_t *mem_icon, *mem_bar, *mem_val;
    lv_obj_t *disk_icon, *disk_bar, *disk_val;
    lv_obj_t *temp_icon, *temp_bar, *temp_val;
    lv_obj_t *net_up_icon, *net_up_val;
    lv_obj_t *net_dn_icon, *net_dn_val;
} overview_widgets_t;

static overview_widgets_t *get_widgets(lv_obj_t *scr) {
    return (overview_widgets_t *)lv_obj_get_user_data(scr);
}

static lv_color_t pct_color(float pct) {
    if (pct > 85.0f) return lv_color_hex(0xf87070);
    if (pct > 65.0f) return lv_color_hex(0xf4a00c);
    return lv_color_hex(0x24d4ec);
}

static lv_color_t temp_color(float deg) {
    if (deg > 65.0f) return lv_color_hex(0xf87070);
    if (deg > 50.0f) return lv_color_hex(0xf4a00c);
    return lv_color_hex(0x34d498);
}

static lv_obj_t *make_row(lv_obj_t *parent, lv_flex_align_t main_align) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return row;
}

static void make_metric_col(lv_obj_t *parent, const lv_image_dsc_t *icon_src,
                             lv_obj_t **icon_out, lv_obj_t **bar_out, lv_obj_t **val_out) {
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_size(col, 0, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_hor(col, 2, 0);
    lv_obj_set_style_pad_ver(col, 4, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_style_pad_row(col, 6, 0);

    lv_obj_t *icon = lv_image_create(col);
    lv_image_set_src(icon, icon_src);
    lv_obj_set_style_image_recolor_opa(icon, LV_OPA_COVER, 0);
    *icon_out = icon;

    lv_obj_t *val = lv_label_create(col);
    lv_label_set_text(val, "---");
    lv_obj_set_style_text_font(val, &jbmono_16, 0);
    lv_obj_set_style_text_color(val, lv_color_hex(0xe8f0f8), 0);
    *val_out = val;

    lv_obj_t *bar = lv_bar_create(col);
    lv_obj_set_size(bar, LV_PCT(80), 2);
    lv_bar_set_range(bar, 0, 100);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x101010), 0);
    lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x24d4ec), LV_PART_INDICATOR);
    *bar_out = bar;
}

lv_obj_t *screen_overview_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_pad_hor(scr, 12, 0);
    lv_obj_set_style_pad_top(scr, 32, 0);
    lv_obj_set_style_pad_bottom(scr, 8, 0);
    lv_obj_set_style_pad_row(scr, 14, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);

    overview_widgets_t *w = static_cast<overview_widgets_t *>(lv_malloc(sizeof(overview_widgets_t)));
    lv_obj_set_user_data(scr, w);

    /* Primary IP */
    w->ip = lv_label_create(scr);
    lv_label_set_text(w->ip, "---");
    lv_obj_set_style_text_font(w->ip, &jbmono_20, 0);
    lv_obj_set_style_text_color(w->ip, lv_color_hex(0x24d4ec), 0);
    lv_obj_set_width(w->ip, LV_PCT(100));

    /* Metrics row: CPU | MEM | DSK | TEMP */
    {
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

        make_metric_col(row, &icon_cpu,  &w->cpu_icon,  &w->cpu_bar,  &w->cpu_val);
        make_metric_col(row, &icon_mem,  &w->mem_icon,  &w->mem_bar,  &w->mem_val);
        make_metric_col(row, &icon_disk, &w->disk_icon, &w->disk_bar, &w->disk_val);
        make_metric_col(row, &icon_temp, &w->temp_icon, &w->temp_bar, &w->temp_val);
    }

    /* Net row: ^ tx    rx v */
    {
        lv_obj_t *row = make_row(scr, LV_FLEX_ALIGN_SPACE_BETWEEN);
        lv_obj_set_flex_grow(row, 1);

        lv_obj_t *up = make_row(row, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_column(up, 6, 0);
        lv_obj_set_size(up, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        w->net_up_icon = lv_image_create(up);
        lv_image_set_src(w->net_up_icon, &icon_chevron_up);
        lv_obj_set_style_image_recolor(w->net_up_icon, lv_color_hex(0x34d498), 0);
        lv_obj_set_style_image_recolor_opa(w->net_up_icon, LV_OPA_COVER, 0);
        w->net_up_val = lv_label_create(up);
        lv_label_set_text(w->net_up_val, "---");
        lv_obj_set_style_text_font(w->net_up_val, &jbmono_14, 0);
        lv_obj_set_style_text_color(w->net_up_val, lv_color_hex(0x34d498), 0);

        lv_obj_t *dn = make_row(row, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_column(dn, 6, 0);
        lv_obj_set_size(dn, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        w->net_dn_val = lv_label_create(dn);
        lv_label_set_text(w->net_dn_val, "---");
        lv_obj_set_style_text_font(w->net_dn_val, &jbmono_14, 0);
        lv_obj_set_style_text_color(w->net_dn_val, lv_color_hex(0x90c4dc), 0);
        w->net_dn_icon = lv_image_create(dn);
        lv_image_set_src(w->net_dn_icon, &icon_chevron_down);
        lv_obj_set_style_image_recolor(w->net_dn_icon, lv_color_hex(0x90c4dc), 0);
        lv_obj_set_style_image_recolor_opa(w->net_dn_icon, LV_OPA_COVER, 0);
    }

    return scr;
}

static void format_bytes_per_sec(char *buf, size_t bufsz, float bps) {
    if (bps >= 1024 * 1024) {
        snprintf(buf, bufsz, "%.1f MB/s", bps / (1024 * 1024));
    } else if (bps >= 1024) {
        snprintf(buf, bufsz, "%.1f KB/s", bps / 1024);
    } else {
        snprintf(buf, bufsz, "%.0f B/s", bps);
    }
}

static void format_mem(char *buf, size_t bufsz, float used_mb, float pct) {
    if (used_mb > 0) {
        if (used_mb >= 1024) {
            snprintf(buf, bufsz, "%.1fG", used_mb / 1024.0f);
        } else {
            snprintf(buf, bufsz, "%.0fM", used_mb);
        }
    } else {
        snprintf(buf, bufsz, "%.0f%%", pct);
    }
}

void screen_overview_update(lv_obj_t *scr, const status_model_t *m) {
    overview_widgets_t *w = get_widgets(scr);
    char buf[64];

    lv_label_set_text(w->ip, m->ip_count > 0 ? m->ips[0] : "---");

    /* CPU */
    lv_bar_set_value(w->cpu_bar, (int32_t)m->cpu, LV_ANIM_ON);
    lv_color_t cpu_c = pct_color(m->cpu);
    lv_obj_set_style_bg_color(w->cpu_bar, cpu_c, LV_PART_INDICATOR);
    lv_obj_set_style_image_recolor(w->cpu_icon, cpu_c, 0);
    snprintf(buf, sizeof(buf), "%.0f%%", m->cpu);
    lv_label_set_text(w->cpu_val, buf);

    /* MEM */
    lv_bar_set_value(w->mem_bar, (int32_t)m->mem, LV_ANIM_ON);
    lv_color_t mem_c = pct_color(m->mem);
    lv_obj_set_style_bg_color(w->mem_bar, mem_c, LV_PART_INDICATOR);
    lv_obj_set_style_image_recolor(w->mem_icon, mem_c, 0);
    format_mem(buf, sizeof(buf), m->mem_used_mb, m->mem);
    lv_label_set_text(w->mem_val, buf);

    /* DISK */
    lv_bar_set_value(w->disk_bar, (int32_t)m->disk, LV_ANIM_ON);
    lv_color_t disk_c = pct_color(m->disk);
    lv_obj_set_style_bg_color(w->disk_bar, disk_c, LV_PART_INDICATOR);
    lv_obj_set_style_image_recolor(w->disk_icon, disk_c, 0);
    snprintf(buf, sizeof(buf), "%.0f%%", m->disk);
    lv_label_set_text(w->disk_val, buf);

    /* TEMP */
    lv_bar_set_value(w->temp_bar, (int32_t)m->temp, LV_ANIM_ON);
    lv_color_t temp_c = temp_color(m->temp);
    lv_obj_set_style_bg_color(w->temp_bar, temp_c, LV_PART_INDICATOR);
    lv_obj_set_style_image_recolor(w->temp_icon, temp_c, 0);
    snprintf(buf, sizeof(buf), "%.0f\xc2\xb0", m->temp);
    lv_label_set_text(w->temp_val, buf);

    /* Net */
    char nbuf[32];
    format_bytes_per_sec(nbuf, sizeof(nbuf), m->net_tx);
    lv_label_set_text(w->net_up_val, nbuf);

    format_bytes_per_sec(nbuf, sizeof(nbuf), m->net_rx);
    lv_label_set_text(w->net_dn_val, nbuf);
}
