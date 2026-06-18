#include "screen_overview.h"
#include "hal.h"
#include <stdio.h>
#include <string.h>

/* Layout: 240 wide x 280 tall (portrait)
 *
 *  ┌──────────────────────┐
 *  │ hostname      ● 48°C │  row: hostname + status dot + temp
 *  │ 192.168.1.42         │  row: primary IP (large)
 *  │ 10.0.0.15            │  row: secondary IPs (small)
 *  │ eth0 · 1Gbps         │  row: interface info
 *  ├──────────────────────┤
 *  │ CPU ████░░░░░  12%   │  bar: cpu
 *  │ MEM ██████░░░  63%   │  bar: mem
 *  │ DSK ███████░░  71%   │  bar: disk
 *  ├──────────────────────┤
 *  │ ↑ 1.2 MB/s           │  net tx
 *  │ ↓ 4.8 MB/s           │  net rx
 *  │ up 2d 4h             │  uptime
 *  └──────────────────────┘
 */

typedef struct {
    lv_obj_t *hostname;
    lv_obj_t *status_dot;
    lv_obj_t *temp;
    lv_obj_t *ip_primary;
    lv_obj_t *ip_secondary;
    lv_obj_t *iface;
    lv_obj_t *cpu_bar;
    lv_obj_t *cpu_label;
    lv_obj_t *mem_bar;
    lv_obj_t *mem_label;
    lv_obj_t *disk_bar;
    lv_obj_t *disk_label;
    lv_obj_t *net_tx;
    lv_obj_t *net_rx;
    lv_obj_t *uptime;
} overview_widgets_t;

static overview_widgets_t *get_widgets(lv_obj_t *scr) {
    return (overview_widgets_t *)lv_obj_get_user_data(scr);
}

static lv_obj_t *add_bar_row(lv_obj_t *parent, const char *label_text,
                              lv_obj_t **bar_out, lv_obj_t **pct_out) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_PCT(100), 22);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_width(lbl, 36);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);

    lv_obj_t *bar = lv_bar_create(row);
    lv_obj_set_size(bar, LV_PCT(70), 10);
    lv_bar_set_range(bar, 0, 100);
    *bar_out = bar;

    lv_obj_t *pct = lv_label_create(row);
    lv_obj_set_style_text_font(pct, &lv_font_montserrat_12, 0);
    lv_label_set_text(pct, " 0%");
    *pct_out = pct;

    return row;
}

lv_obj_t *screen_overview_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_pad_all(scr, 6, 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);

    overview_widgets_t *w = static_cast<overview_widgets_t *>(lv_malloc(sizeof(overview_widgets_t)));
    lv_obj_set_user_data(scr, w);

    /* Row 1: hostname + dot + temp */
    lv_obj_t *row1 = lv_obj_create(scr);
    lv_obj_set_size(row1, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    w->hostname = lv_label_create(row1);
    lv_label_set_text(w->hostname, "...");
    lv_obj_set_style_text_font(w->hostname, &lv_font_montserrat_14, 0);
    lv_obj_set_flex_grow(w->hostname, 1);

    w->status_dot = lv_obj_create(row1);
    lv_obj_set_size(w->status_dot, 10, 10);
    lv_obj_set_style_radius(w->status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(w->status_dot, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_style_border_width(w->status_dot, 0, 0);

    w->temp = lv_label_create(row1);
    lv_label_set_text(w->temp, " --°C");
    lv_obj_set_style_text_font(w->temp, &lv_font_montserrat_12, 0);

    /* Primary IP */
    w->ip_primary = lv_label_create(scr);
    lv_label_set_text(w->ip_primary, "---");
    lv_obj_set_style_text_font(w->ip_primary, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(w->ip_primary, lv_palette_main(LV_PALETTE_CYAN), 0);

    /* Secondary IPs */
    w->ip_secondary = lv_label_create(scr);
    lv_label_set_text(w->ip_secondary, "");
    lv_obj_set_style_text_font(w->ip_secondary, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(w->ip_secondary, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);

    /* Interface */
    w->iface = lv_label_create(scr);
    lv_label_set_text(w->iface, "");
    lv_obj_set_style_text_font(w->iface, &lv_font_montserrat_12, 0);

    /* Separator */
    lv_obj_t *sep1 = lv_obj_create(scr);
    lv_obj_set_size(sep1, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(sep1, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_border_width(sep1, 0, 0);

    /* Metric bars */
    add_bar_row(scr, "CPU", &w->cpu_bar, &w->cpu_label);
    add_bar_row(scr, "MEM", &w->mem_bar, &w->mem_label);
    add_bar_row(scr, "DSK", &w->disk_bar, &w->disk_label);

    /* Separator */
    lv_obj_t *sep2 = lv_obj_create(scr);
    lv_obj_set_size(sep2, LV_PCT(100), 1);
    lv_obj_set_style_bg_color(sep2, lv_palette_darken(LV_PALETTE_GREY, 3), 0);
    lv_obj_set_style_border_width(sep2, 0, 0);

    /* Network + uptime */
    w->net_tx = lv_label_create(scr);
    lv_label_set_text(w->net_tx, "");
    lv_obj_set_style_text_font(w->net_tx, &lv_font_montserrat_12, 0);

    w->net_rx = lv_label_create(scr);
    lv_label_set_text(w->net_rx, "");
    lv_obj_set_style_text_font(w->net_rx, &lv_font_montserrat_12, 0);

    w->uptime = lv_label_create(scr);
    lv_label_set_text(w->uptime, "");
    lv_obj_set_style_text_font(w->uptime, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(w->uptime, lv_palette_lighten(LV_PALETTE_GREY, 2), 0);

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

    lv_label_set_text(w->hostname, m->hostname);

    lv_color_t dot_color = m->alert
        ? lv_palette_main(LV_PALETTE_RED)
        : lv_palette_main(LV_PALETTE_GREEN);
    lv_obj_set_style_bg_color(w->status_dot, dot_color, 0);

    snprintf(buf, sizeof(buf), " %.0f°C", m->temp);
    lv_label_set_text(w->temp, buf);

    lv_label_set_text(w->ip_primary, m->ip_count > 0 ? m->ips[0] : "---");

    if (m->ip_count > 1) {
        char ips[MODEL_MAX_IPS * MODEL_MAX_IP_LEN] = {0};
        for (int i = 1; i < m->ip_count; i++) {
            if (i > 1) strncat(ips, "  ", sizeof(ips) - strlen(ips) - 1);
            strncat(ips, m->ips[i], sizeof(ips) - strlen(ips) - 1);
        }
        lv_label_set_text(w->ip_secondary, ips);
    } else {
        lv_label_set_text(w->ip_secondary, "");
    }

    snprintf(buf, sizeof(buf), "%s · %s", m->primary_if, m->link);
    lv_label_set_text(w->iface, buf);

    lv_bar_set_value(w->cpu_bar, (int32_t)m->cpu, LV_ANIM_ON);
    snprintf(buf, sizeof(buf), " %.0f%%", m->cpu);
    lv_label_set_text(w->cpu_label, buf);

    lv_bar_set_value(w->mem_bar, (int32_t)m->mem, LV_ANIM_ON);
    snprintf(buf, sizeof(buf), " %.0f%%", m->mem);
    lv_label_set_text(w->mem_label, buf);

    lv_bar_set_value(w->disk_bar, (int32_t)m->disk, LV_ANIM_ON);
    snprintf(buf, sizeof(buf), " %.0f%%", m->disk);
    lv_label_set_text(w->disk_label, buf);

    char net_buf[32];
    format_bytes_per_sec(net_buf, sizeof(net_buf), m->net_tx);
    snprintf(buf, sizeof(buf), "↑ %s", net_buf);
    lv_label_set_text(w->net_tx, buf);

    format_bytes_per_sec(net_buf, sizeof(net_buf), m->net_rx);
    snprintf(buf, sizeof(buf), "↓ %s", net_buf);
    lv_label_set_text(w->net_rx, buf);

    snprintf(buf, sizeof(buf), "up %s", m->uptime);
    lv_label_set_text(w->uptime, buf);
}
