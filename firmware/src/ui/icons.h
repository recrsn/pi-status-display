#pragma once

#include "lvgl.h"

/* A8 (alpha-only) icon bitmaps rasterized from the reference design's SVGs.
 * Being alpha-only, they take a stroke color via lv_obj_set_style_image_recolor
 * + LV_OPA_COVER, same technique as tinting a monochrome font glyph. */

#ifdef __cplusplus
extern "C" {
#endif

LV_IMAGE_DECLARE(icon_cpu);
LV_IMAGE_DECLARE(icon_mem);
LV_IMAGE_DECLARE(icon_disk);
LV_IMAGE_DECLARE(icon_temp);
LV_IMAGE_DECLARE(icon_chevron_up);
LV_IMAGE_DECLARE(icon_chevron_down);
LV_IMAGE_DECLARE(icon_wifi);
LV_IMAGE_DECLARE(icon_eth);
LV_IMAGE_DECLARE(icon_server);

#ifdef __cplusplus
}
#endif
