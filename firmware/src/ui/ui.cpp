#include "ui.hpp"
#include "hal.h"
#include "fonts.h"
#include "statusbar.h"
#include "screen_overview.h"
#include "screen_network.h"
#include "screen_services.h"
#include <string.h>
#include <stdio.h>
#include <atomic>
#include <lvgl.h>

#define DATA_TIMEOUT_MS 3000

#define SCR_OVERVIEW 0
#define SCR_NETWORK  1
#define SCR_SERVICES 2
#define SCR_COUNT    3

/* Active content screens, index matches SCR_* constants */
static lv_obj_t *screens[SCR_COUNT];
static lv_obj_t *scr_connecting;
static int       current_screen = SCR_OVERVIEW;

/* Lock-free single-producer/single-consumer ring buffer handing fully
 * parsed models from the RX task (usb_rx_task / socket reader_thread) to
 * ui_tick() on the LVGL-owning task. ui_on_data() runs entirely on the RX
 * task (pinned to core 1, opposite LVGL render + DMA on core 0, see
 * hal_esp32.c) — it parses JSON there via model_parse() into a scratch
 * struct, so cJSON's cost never competes with core 0's render/flush work.
 * status_model_t is a flat POD struct (no pointers), so a snapshot copy
 * into a ring slot is all that's needed to hand it across cores; only the
 * RX task ever writes model_parse()'s dst, only ui_tick() ever touches
 * widgets. head/tail are monotonic counters; a slot at index i is safe for
 * the consumer to read once it observes head > i, and safe for the producer
 * to reuse once it observes tail > i - RX_RING_SIZE. */
#define RX_RING_SIZE 4

struct rx_slot_t {
    status_model_t model;
};

static rx_slot_t g_rx_ring[RX_RING_SIZE];
static std::atomic<uint32_t> g_rx_head{0};
static std::atomic<uint32_t> g_rx_tail{0};

/* Page-dot widgets on lv_layer_top(), one per screen */
static lv_obj_t *dots[SCR_COUNT];

/* One gesture per touch: reset when pointer is released */
static bool gesture_consumed = false;

/* ── Page dots ──────────────────────────────────────────────────────────── */

static void create_page_dots(void) {
    lv_obj_t *layer = lv_layer_top();

    lv_obj_t *bar = lv_obj_create(layer);
    lv_obj_set_size(bar, LV_PCT(100), 18);
    lv_obj_align(bar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, 5, 0);

    for (int i = 0; i < SCR_COUNT; i++) {
        dots[i] = lv_obj_create(bar);
        lv_obj_set_height(dots[i], 2);
        lv_obj_set_style_border_width(dots[i], 0, 0);
        lv_obj_set_style_radius(dots[i], 0, 0);
    }
}

static void update_page_dots(int active) {
    for (int i = 0; i < SCR_COUNT; i++) {
        bool is_active = (i == active);
        lv_obj_set_width(dots[i], is_active ? 16 : 4);
        lv_obj_set_style_bg_color(dots[i],
            is_active ? lv_color_hex(0x24d4ec) : lv_color_hex(0x1c2030), 0);
    }
}

/* ── Swipe navigation ───────────────────────────────────────────────────── */

static void navigate(int delta) {
    int next = current_screen + delta;
    if (next < 0 || next >= SCR_COUNT) return;

    lv_scr_load_anim_t anim = (delta > 0)
        ? LV_SCR_LOAD_ANIM_MOVE_LEFT
        : LV_SCR_LOAD_ANIM_MOVE_RIGHT;

    lv_screen_load_anim(screens[next], anim, 200, 0, false);
    current_screen = next;
    update_page_dots(current_screen);
}

static void gesture_cb(lv_event_t *e) {
    (void)e;
    if (gesture_consumed) return;

    lv_indev_t *indev = lv_indev_active();
    if (!indev) return;

    lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir == LV_DIR_LEFT)       { gesture_consumed = true; navigate(+1); }
    else if (dir == LV_DIR_RIGHT) { gesture_consumed = true; navigate(-1); }
}

static lv_obj_t *make_connecting_screen(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "CONNECTING...");
    lv_obj_set_style_text_font(label, &jbmono_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x3c5878), 0);
    lv_obj_set_style_text_letter_space(label, 1, 0);
    lv_obj_center(label);
    return scr;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

void ui_init(void) {
    scr_connecting = make_connecting_screen();

    screens[SCR_OVERVIEW] = screen_overview_create();
    screens[SCR_NETWORK]  = screen_network_create();
    screens[SCR_SERVICES] = screen_services_create();

    /* Register swipe handler on every content screen */
    for (int i = 0; i < SCR_COUNT; i++) {
        lv_obj_add_event_cb(screens[i], gesture_cb, LV_EVENT_GESTURE, NULL);
    }

    statusbar_create();
    create_page_dots();
    update_page_dots(SCR_OVERVIEW);

    lv_screen_load(scr_connecting);
}

/* Runs on the RX task (core 1). Parses JSON here — kept off core 0 so it
 * never competes with LVGL render/flush dispatch or the SPI DMA ISR, both
 * of which run on core 0 (see hal_esp32.c). `staging` persists across calls
 * (same task, single producer) so a packet that omits a field leaves the
 * prior value in place, exactly as when parsing wrote directly into the
 * shared model. Only the fully parsed snapshot crosses to the consumer. */
void ui_on_data(const char *json, size_t len) {
    static status_model_t staging = {};

    if (!model_parse(&staging, json, len)) return;

    staging.last_update_ms = hal_tick_ms();

    /* Only the latest state matters, so if the consumer has fallen behind
     * (unrealistic at ~1 pkt/s) the whole unread backlog is dropped rather
     * than the new snapshot. */
    uint32_t head = g_rx_head.load(std::memory_order_relaxed);
    uint32_t tail = g_rx_tail.load(std::memory_order_acquire);
    if (head - tail >= RX_RING_SIZE) g_rx_tail.store(head, std::memory_order_release);

    g_rx_ring[head % RX_RING_SIZE].model = staging;

    g_rx_head.store(head + 1, std::memory_order_release);
}

/* Applies a freshly parsed model to the widgets. Runs on the LVGL-owning
 * task only, called from ui_tick(). */
static void apply_model_update(status_model_t *m) {
    lv_display_t *disp = lv_display_get_default();
    lv_display_enable_invalidation(disp, false);

    statusbar_update(m);

    if (lv_screen_active() == scr_connecting) {
        current_screen = SCR_OVERVIEW;
        update_page_dots(current_screen);
        lv_screen_load_anim(screens[SCR_OVERVIEW], LV_SCR_LOAD_ANIM_FADE_IN, 200, 0, false);
    }

    screen_overview_update(screens[SCR_OVERVIEW], m);
    screen_network_update(screens[SCR_NETWORK], m);
    screen_services_update(screens[SCR_SERVICES], m);

    /* Verified empirically: letting each widget setter's own small-area
     * invalidate stand, instead of merging into one full-screen invalidate,
     * drove flush count from 1/tick to ~18/tick — each scattered dirty rect
     * (bar, label, icon) pays its own ~3-4ms fixed SPI-dispatch overhead,
     * same failure mode the PARTIAL-mode buffer had. One full-screen flush
     * transfers more bytes but pays that overhead once, and wins at the
     * current 80MHz SPI clock. */
    lv_display_enable_invalidation(disp, true);
    lv_obj_invalidate(lv_screen_active());
    lv_obj_invalidate(lv_layer_top());
}

/* Drains every model snapshot queued since the last tick. Parsing already
 * happened on the RX task (core 1); this just publishes the snapshot to the
 * shared status_model_t and applies it to widgets, on the LVGL-owning task
 * (core 0) only. */
static void drain_rx_ring(status_model_t *m) {
    uint32_t tail = g_rx_tail.load(std::memory_order_relaxed);
    uint32_t head = g_rx_head.load(std::memory_order_acquire);

    while (tail != head) {
        *m = g_rx_ring[tail % RX_RING_SIZE].model;
        apply_model_update(m);
        tail++;
    }
    g_rx_tail.store(tail, std::memory_order_release);
}

void ui_tick(void) {
    status_model_t *m = model_get();

    /* Reset gesture flag when pointer is released */
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            if (lv_indev_get_state(indev) == LV_INDEV_STATE_RELEASED) {
                gesture_consumed = false;
            }
        }
        indev = lv_indev_get_next(indev);
    }

    drain_rx_ring(m);

    if (m->valid && (hal_tick_ms() - m->last_update_ms) > DATA_TIMEOUT_MS) {
        if (lv_screen_active() != scr_connecting) {
            lv_screen_load(scr_connecting);
        }
    }

    lv_timer_handler();
}
