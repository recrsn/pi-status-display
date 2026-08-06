#include "screen_network.h"
#include "fonts.h"
#include "icons.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Layout: 240 x 280, status bar (28px) overlays the top.
 *
 *  ┌────────────────────────────┐
 *  │ (eth) eth0             ● UP│
 *  │ 192.168.1.42                │
 *  │ ^ 214.6 KB/s   v 1.4 MB/s   │
 *  │                             │
 *  │ (wifi) wlan0 SSID  ▂▃▄▅ ● UP│
 *  │ 192.168.1.43                │
 *  │ ^ 348.6 KB/s   v 296.2 KB/s │
 *  │                             │
 *  └────────────────────────────┘
 *
 * Two fixed interface slots (matches the design + available vertical
 * space); a slot is hidden entirely when the agent reports fewer
 * interfaces than that.
 */

#define NET_IFACE_SLOTS 2
#define NET_SIGNAL_BARS 4

typedef struct {
    lv_obj_t *section;
    lv_obj_t *icon;
    lv_obj_t *name;
    lv_obj_t *ssid;
    lv_obj_t *bars[NET_SIGNAL_BARS];
    lv_obj_t *dot;
    lv_obj_t *status;
    lv_obj_t *ip;
    lv_obj_t *net_row;
    lv_obj_t *tx_icon, *tx_val;
    lv_obj_t *rx_icon, *rx_val;
} iface_widgets_t;

typedef struct {
    iface_widgets_t ifaces[NET_IFACE_SLOTS];
} network_widgets_t;

static network_widgets_t *get_widgets(lv_obj_t *scr) {
    return (network_widgets_t *)lv_obj_get_user_data(scr);
}

static lv_obj_t *make_row(lv_obj_t *parent, lv_flex_align_t main_align) {
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return row;
}

static iface_widgets_t make_iface_section(lv_obj_t *parent) {
    iface_widgets_t w = {0};

    w.section = lv_obj_create(parent);
    lv_obj_remove_style_all(w.section);
    lv_obj_set_size(w.section, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(w.section, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(w.section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(w.section, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(w.section, 5, 0);
    lv_obj_set_flex_grow(w.section, 1);

    /* Row 1: icon + name + ssid + signal bars ......... status dot */
    {
        lv_obj_t *row = lv_obj_create(w.section);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *left = make_row(row, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_column(left, 6, 0);

        w.icon = lv_image_create(left);
        lv_image_set_src(w.icon, &icon_eth);
        lv_obj_set_style_image_recolor(w.icon, lv_color_hex(0x3c5878), 0);
        lv_obj_set_style_image_recolor_opa(w.icon, LV_OPA_COVER, 0);

        w.name = lv_label_create(left);
        lv_label_set_text(w.name, "---");
        lv_obj_set_style_text_font(w.name, &jbmono_10, 0);
        lv_obj_set_style_text_color(w.name, lv_color_hex(0x3c5878), 0);
        lv_obj_set_style_text_letter_space(w.name, 1, 0);

        w.ssid = lv_label_create(left);
        lv_label_set_text(w.ssid, "");
        lv_obj_set_style_text_font(w.ssid, &jbmono_10, 0);
        lv_obj_set_style_text_color(w.ssid, lv_color_hex(0x3c5878), 0);

        lv_obj_t *bars_box = make_row(left, LV_FLEX_ALIGN_END);
        lv_obj_set_style_pad_column(bars_box, 2, 0);
        for (int i = 0; i < NET_SIGNAL_BARS; i++) {
            w.bars[i] = lv_obj_create(bars_box);
            lv_obj_remove_style_all(w.bars[i]);
            lv_obj_set_size(w.bars[i], 3, 2 + (i + 1) * 2);
            lv_obj_align(w.bars[i], LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_set_style_bg_opa(w.bars[i], LV_OPA_COVER, 0);
            lv_obj_set_style_bg_color(w.bars[i], lv_color_hex(0x1c2030), 0);
        }

        lv_obj_t *status_box = make_row(row, LV_FLEX_ALIGN_START);
        lv_obj_set_style_pad_column(status_box, 5, 0);

        w.dot = lv_obj_create(status_box);
        lv_obj_remove_style_all(w.dot);
        lv_obj_set_size(w.dot, 5, 5);
        lv_obj_set_style_radius(w.dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(w.dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(w.dot, lv_color_hex(0x34d498), 0);

        w.status = lv_label_create(status_box);
        lv_label_set_text(w.status, "UP");
        lv_obj_set_style_text_font(w.status, &jbmono_10, 0);
        lv_obj_set_style_text_color(w.status, lv_color_hex(0x34d498), 0);
        lv_obj_set_style_text_letter_space(w.status, 1, 0);
    }

    /* IP - large cyan */
    w.ip = lv_label_create(w.section);
    lv_label_set_text(w.ip, "---");
    lv_obj_set_style_text_font(w.ip, &jbmono_16, 0);
    lv_obj_set_style_text_color(w.ip, lv_color_hex(0x24d4ec), 0);
    lv_obj_set_width(w.ip, LV_PCT(100));
    lv_label_set_long_mode(w.ip, LV_LABEL_LONG_CLIP);

    /* TX / RX row */
    w.net_row = lv_obj_create(w.section);
    lv_obj_remove_style_all(w.net_row);
    lv_obj_set_size(w.net_row, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_clear_flag(w.net_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(w.net_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(w.net_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *tx = make_row(w.net_row, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(tx, 4, 0);
    w.tx_icon = lv_image_create(tx);
    lv_image_set_src(w.tx_icon, &icon_chevron_up);
    lv_obj_set_style_image_recolor(w.tx_icon, lv_color_hex(0x34d498), 0);
    lv_obj_set_style_image_recolor_opa(w.tx_icon, LV_OPA_COVER, 0);
    w.tx_val = lv_label_create(tx);
    lv_label_set_text(w.tx_val, "---");
    lv_obj_set_style_text_font(w.tx_val, &jbmono_10, 0);
    lv_obj_set_style_text_color(w.tx_val, lv_color_hex(0x34d498), 0);

    lv_obj_t *rx = make_row(w.net_row, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(rx, 4, 0);
    w.rx_icon = lv_image_create(rx);
    lv_image_set_src(w.rx_icon, &icon_chevron_down);
    lv_obj_set_style_image_recolor(w.rx_icon, lv_color_hex(0x90c4dc), 0);
    lv_obj_set_style_image_recolor_opa(w.rx_icon, LV_OPA_COVER, 0);
    w.rx_val = lv_label_create(rx);
    lv_label_set_text(w.rx_val, "---");
    lv_obj_set_style_text_font(w.rx_val, &jbmono_10, 0);
    lv_obj_set_style_text_color(w.rx_val, lv_color_hex(0x90c4dc), 0);

    return w;
}

lv_obj_t *screen_network_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_pad_hor(scr, 12, 0);
    lv_obj_set_style_pad_top(scr, 32, 0);
    lv_obj_set_style_pad_bottom(scr, 8, 0);
    lv_obj_set_style_pad_row(scr, 6, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);

    network_widgets_t *w = static_cast<network_widgets_t *>(lv_malloc(sizeof(network_widgets_t)));
    memset(w, 0, sizeof(*w));
    lv_obj_set_user_data(scr, w);

    for (int i = 0; i < NET_IFACE_SLOTS; i++) {
        w->ifaces[i] = make_iface_section(scr);
    }

    return scr;
}

static void format_bps(char *buf, size_t sz, float bps) {
    if (bps >= 1024 * 1024) snprintf(buf, sz, "%.1f MB/s", bps / (1024 * 1024));
    else if (bps >= 1024)   snprintf(buf, sz, "%.1f KB/s", bps / 1024);
    else                    snprintf(buf, sz, "%.0f B/s", bps);
}

static void update_iface_section(iface_widgets_t *w, const iface_t *iface) {
    lv_obj_clear_flag(w->section, LV_OBJ_FLAG_HIDDEN);

    bool is_wifi = strcmp(iface->type, "wifi") == 0;
    lv_image_set_src(w->icon, is_wifi ? &icon_wifi : &icon_eth);

    char name_buf[MODEL_MAX_IFACE + 1];
    for (size_t i = 0; i < sizeof(name_buf) - 1 && iface->name[i]; i++) {
        char c = iface->name[i];
        name_buf[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
        name_buf[i + 1] = '\0';
    }
    lv_label_set_text(w->name, iface->name[0] ? name_buf : "---");

    if (is_wifi && iface->ssid[0]) {
        lv_label_set_text(w->ssid, iface->ssid);
        lv_obj_clear_flag(w->ssid, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(w->ssid, LV_OBJ_FLAG_HIDDEN);
    }

    if (is_wifi && iface->has_signal) {
        int strength = (int)lroundf(((iface->signal + 90.0f) / 60.0f) * NET_SIGNAL_BARS);
        for (int i = 0; i < NET_SIGNAL_BARS; i++) {
            lv_obj_clear_flag(w->bars[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_color(w->bars[i],
                (i + 1) <= strength ? lv_color_hex(0x24d4ec) : lv_color_hex(0x1c2030), 0);
        }
    } else {
        for (int i = 0; i < NET_SIGNAL_BARS; i++) lv_obj_add_flag(w->bars[i], LV_OBJ_FLAG_HIDDEN);
    }

    lv_color_t status_color = iface->up ? lv_color_hex(0x34d498) : lv_color_hex(0xf87070);
    lv_obj_set_style_bg_color(w->dot, status_color, 0);
    lv_obj_set_style_text_color(w->status, status_color, 0);
    lv_label_set_text(w->status, iface->up ? "UP" : "DOWN");

    lv_label_set_text(w->ip, iface->ip[0] ? iface->ip : "---");

    if (iface->up) {
        char nbuf[32];
        format_bps(nbuf, sizeof(nbuf), iface->tx_rate);
        lv_label_set_text(w->tx_val, nbuf);
        format_bps(nbuf, sizeof(nbuf), iface->rx_rate);
        lv_label_set_text(w->rx_val, nbuf);
        lv_obj_clear_flag(w->net_row, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(w->net_row, LV_OBJ_FLAG_HIDDEN);
    }
}

void screen_network_update(lv_obj_t *scr, const status_model_t *m) {
    network_widgets_t *w = get_widgets(scr);

    for (int i = 0; i < NET_IFACE_SLOTS; i++) {
        if (i < m->interface_count) {
            update_iface_section(&w->ifaces[i], &m->interfaces[i]);
        } else {
            lv_obj_add_flag(w->ifaces[i].section, LV_OBJ_FLAG_HIDDEN);
        }
    }
}
