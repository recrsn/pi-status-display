/*
 * Shared LVGL v9 configuration for firmware and emulator targets.
 * Do NOT include C system headers here: this file is preprocessed by
 * lv_blend_helium.S and other assembly files; C structs break the assembler.
 */

#if 1  /* guard required by LVGL */

#ifndef LV_CONF_H
#define LV_CONF_H

/* Memory -------------------------------------------------------------------*/
#ifdef LVGL_EMULATOR
#  define LV_MEM_SIZE (4U * 1024U * 1024U)
#else
#  define LV_MEM_SIZE (128U * 1024U)
#endif
#define LV_MEM_POOL_INCLUDE <stdlib.h>
#define LV_MEM_POOL_ALLOC   malloc
#define LV_MEM_POOL_FREE    free

/* Color ----------------------------------------------------------------------
 * LVGL's RGB565 framebuffer is native (little-endian) byte order. The
 * ST7789V2 is fed raw RGB565 over SPI and expects big-endian (MSB first)
 * per pixel, so the firmware target needs the bytes swapped before flush;
 * SDL takes the buffer as-is on the host's native order. */
#define LV_COLOR_DEPTH 16
#ifdef LVGL_EMULATOR
#  define LV_COLOR_16_SWAP 0
#else
#  define LV_COLOR_16_SWAP 1
#endif

/* Tick: SDL driver calls lv_tick_set_cb(SDL_GetTicks) automatically.
 * Firmware calls lv_tick_set_cb(hal_tick_ms) in hal_init(). */

/* Logging ------------------------------------------------------------------
 * On the esp32s3 target, LV_LOG_PRINTF's direct vprintf() call goes out over
 * the same USB-Serial-JTAG wire our own driver owns; an LVGL log firing
 * while that driver's TX interrupt is in flight hits a queue-state assert
 * and reboots the board. Emulator keeps console logging since it has no such
 * conflict. */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#ifdef LVGL_EMULATOR
#  define LV_LOG_PRINTF 1
#else
#  define LV_LOG_PRINTF 0
#endif

/* SDL driver (emulator only) --------------------------------------------*/
#ifdef LVGL_EMULATOR
#  define LV_USE_SDL 1
#  define LV_SDL_INCLUDE_PATH <SDL2/SDL.h>
#else
#  define LV_USE_SDL 0
#endif

/* Fonts -----------------------------------------------------------------*/
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Widgets ---------------------------------------------------------------*/
#define LV_USE_LABEL  1
#define LV_USE_ARC    1
#define LV_USE_BAR    1
#define LV_USE_TABLE  1

#endif /* LV_CONF_H */
#endif /* guard */
