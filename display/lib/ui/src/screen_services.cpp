#include "screen_services.h"
#include <string.h>

lv_obj_t *screen_services_create(void) {
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_pad_all(scr, 6, 0);
    lv_obj_set_style_text_color(scr, lv_color_white(), 0);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Services");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    /* Table populated on each update */
    lv_obj_t *table = lv_table_create(scr);
    lv_table_set_col_cnt(table, 2);
    lv_table_set_col_width(table, 0, 170);
    lv_table_set_col_width(table, 1, 60);
    lv_obj_set_style_bg_color(table, lv_color_black(), 0);
    lv_obj_set_style_text_color(table, lv_color_white(), 0);
    lv_obj_set_style_border_width(table, 0, LV_PART_ITEMS);
    lv_obj_set_user_data(scr, table);

    return scr;
}

void screen_services_update(lv_obj_t *scr, const status_model_t *m) {
    lv_obj_t *table = (lv_obj_t *)lv_obj_get_user_data(scr);

    lv_table_set_row_cnt(table, (uint16_t)m->service_count);

    for (int i = 0; i < m->service_count; i++) {
        const service_t *s = &m->services[i];
        lv_table_set_cell_value(table, (uint16_t)i, 0, s->name);
        lv_table_set_cell_value(table, (uint16_t)i, 1, s->active ? "●" : "○");

        lv_color_t col = s->active
            ? lv_palette_main(LV_PALETTE_GREEN)
            : lv_palette_main(LV_PALETTE_RED);
        /* Color the status cell via a draw event — set via part/state style */
        lv_obj_set_style_text_color(table, col,
            LV_PART_ITEMS | LV_STATE_DEFAULT);
    }
}
