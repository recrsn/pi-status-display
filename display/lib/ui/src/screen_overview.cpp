#include "screen_overview.h"
#include "hal.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Layout: 240 wide × 280 tall (portrait)
 *
 *  ┌────────────────────────────┐
 *  │ hostname (uptime)   HH:MM  │
 *  │ 192.168.1.42               │
 *  ├────────────────────────────┤
 *  │  CPU  │  MEM  │  DSK │TEMP │
 *  │ ████░ │ ████░ │ ████░│ 52° │
 *  │  45%  │  63%  │  71% │     │
 *  ├────────────────────────────┤
 *  │ ↑ 1.2 MB/s    ↓ 4.8 MB/s  │
 *  └────────────────────────────┘
 */

typedef struct {
    lv_obj_t *hostname_uptime;
    lv_obj_t *time_label;
    lv_obj_t *ip;
    lv_obj_t *cpu_bar;
    lv_obj_t *cpu_val;
    lv_obj_t *mem_bar;
    lv_obj_t *mem_val;
    lv_obj_t *disk_bar;
    lv_obj_t *disk_val;
    lv_obj_t *temp_val;
    lv_obj_t *net_up;
    lv_obj_t *net_dn;
} overview_widgets_t;

static overview_widgets_t *get_widgets(lv_obj_t *scr) {
    return (overview_widgets_t *)lv_obj_get_user_data(scr);
}

static lv_obj_t *make_row(lv_obj_t *parent, lv_flex_align_t main_align) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return row;
}

static lv_obj_t *make_hsep(lv_obj_t *parent) {
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_set_size(sep, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(sep, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    return sep;
}

static lv_obj_t *make_vsep(lv_obj_t *parent, int32_t h) {
    lv_obj_t *sep = lv_obj_create(parent);
    lv_obj_set_size(sep, 1, h);
    lv_obj_set_style_bg_color(sep, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_border_width(sep, 0, 0);
    return sep;
}

static void make_metric_col(lv_obj_t *parent, const char *title,
                             bool has_bar,
                             lv_obj_t **bar_out, lv_obj_t **val_out) {
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_size(col, 0, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(col, 4, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_flex_grow(col, 1);

    lv_obj_t *lbl = lv_label_create(col);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);

    if (has_bar) {
        lv_obj_t *bar = lv_bar_create(col);
        lv_obj_set_size(bar, LV_PCT(90), 8);
        lv_bar_set_range(bar, 0, 100);
        lv_obj_set_style_margin_top(bar, 3, 0);
        if (bar_out) *bar_out = bar;
    } else {
        if (bar_out) *bar_out = nullptr;
    }

    lv_obj_t *val = lv_label_create(col);
    lv_label_set_text(val, "---");
    lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_margin_top(val, 3, 0);
    *val_out = val;
}

lv_obj_t *screen_overview_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_pad_all(scr, 8, 0);
    lv_obj_set_style_pad_row(scr, 6, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);

    overview_widgets_t *w = static_cast<overview_widgets_t *>(lv_malloc(sizeof(overview_widgets_t)));
    lv_obj_set_user_data(scr, w);

    /* Row 1: "hostname (uptime)"   "HH:MM" */
    {
        lv_obj_t *row = make_row(scr, LV_FLEX_ALIGN_SPACE_BETWEEN);

        w->hostname_uptime = lv_label_create(row);
        lv_label_set_text(w->hostname_uptime, "...");
        lv_obj_set_style_text_font(w->hostname_uptime, &lv_font_montserrat_14, 0);
        lv_obj_set_flex_grow(w->hostname_uptime, 1);
        lv_label_set_long_mode(w->hostname_uptime, LV_LABEL_LONG_CLIP);

        w->time_label = lv_label_create(row);
        lv_label_set_text(w->time_label, "--:--");
        lv_obj_set_style_text_font(w->time_label, &lv_font_montserrat_18, 0);
    }

    /* Row 2: primary IP */
    w->ip = lv_label_create(scr);
    lv_label_set_text(w->ip, "---");
    lv_obj_set_style_text_font(w->ip, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(w->ip, lv_palette_main(LV_PALETTE_CYAN), 0);
    lv_obj_set_width(w->ip, LV_PCT(100));

    make_hsep(scr);

    /* Metrics row: CPU | MEM | DSK | TEMP */
    {
        lv_obj_t *row = lv_obj_create(scr);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        make_metric_col(row, "CPU",  true,  &w->cpu_bar,  &w->cpu_val);
        make_vsep(row, 60);
        make_metric_col(row, "MEM",  true,  &w->mem_bar,  &w->mem_val);
        make_vsep(row, 60);
        make_metric_col(row, "DSK",  true,  &w->disk_bar, &w->disk_val);
        make_vsep(row, 60);
        make_metric_col(row, "TEMP", false, nullptr,      &w->temp_val);
    }

    make_hsep(scr);

    /* Net row: ↑ tx    ↓ rx */
    {
        lv_obj_t *row = make_row(scr, LV_FLEX_ALIGN_SPACE_BETWEEN);

        w->net_up = lv_label_create(row);
        lv_label_set_text(w->net_up, "↑ ---");
        lv_obj_set_style_text_font(w->net_up, &lv_font_montserrat_14, 0);

        w->net_dn = lv_label_create(row);
        lv_label_set_text(w->net_dn, "↓ ---");
        lv_obj_set_style_text_font(w->net_dn, &lv_font_montserrat_14, 0);
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

void screen_overview_update(lv_obj_t *scr, const status_model_t *m) {
    overview_widgets_t *w = get_widgets(scr);
    char buf[64];

    snprintf(buf, sizeof(buf), "%s  (%s)", m->hostname, m->uptime);
    lv_label_set_text(w->hostname_uptime, buf);

    if (m->ts > 0) {
        time_t t = (time_t)m->ts;
        struct tm *tm_info = localtime(&t);
        strftime(buf, sizeof(buf), "%H:%M", tm_info);
    } else {
        snprintf(buf, sizeof(buf), "--:--");
    }
    lv_label_set_text(w->time_label, buf);

    lv_label_set_text(w->ip, m->ip_count > 0 ? m->ips[0] : "---");

    lv_bar_set_value(w->cpu_bar, (int32_t)m->cpu, LV_ANIM_ON);
    snprintf(buf, sizeof(buf), "%.0f%%", m->cpu);
    lv_label_set_text(w->cpu_val, buf);

    lv_bar_set_value(w->mem_bar, (int32_t)m->mem, LV_ANIM_ON);
    snprintf(buf, sizeof(buf), "%.0f%%", m->mem);
    lv_label_set_text(w->mem_val, buf);

    lv_bar_set_value(w->disk_bar, (int32_t)m->disk, LV_ANIM_ON);
    snprintf(buf, sizeof(buf), "%.0f%%", m->disk);
    lv_label_set_text(w->disk_val, buf);

    snprintf(buf, sizeof(buf), "%.0f\xc2\xb0\x43", m->temp);  /* °C in UTF-8 */
    lv_label_set_text(w->temp_val, buf);
    lv_obj_set_style_text_color(w->temp_val,
        m->alert ? lv_palette_main(LV_PALETTE_RED) : lv_color_white(), 0);

    char nbuf[32];
    format_bytes_per_sec(nbuf, sizeof(nbuf), m->net_tx);
    snprintf(buf, sizeof(buf), "\xe2\x86\x91 %s", nbuf);  /* ↑ */
    lv_label_set_text(w->net_up, buf);

    format_bytes_per_sec(nbuf, sizeof(nbuf), m->net_rx);
    snprintf(buf, sizeof(buf), "\xe2\x86\x93 %s", nbuf);  /* ↓ */
    lv_label_set_text(w->net_dn, buf);
}
