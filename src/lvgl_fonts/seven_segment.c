/*******************************************************************************
 * Size: 4 px
 * Bpp: 8
 * Opts: --bpp 8 --size 4 --no-compress --font 7-segments.ttf --symbols  -.0123456789:_` --format lvgl -o seven_segment.c
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef SEVEN_SEGMENT
#define SEVEN_SEGMENT 1
#endif

#if SEVEN_SEGMENT

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t glyph_bitmap[] = {
    /* U+0020 " " */

    /* U+002D "-" */
    0x3f, 0xff, 0xff, 0xff, 0x40,

    /* U+002E "." */
    0xb8,

    /* U+0030 "0" */
    0x7f, 0xff, 0xff, 0x80, 0xff, 0x0, 0x0, 0xff,
    0xff, 0x0, 0x0, 0xff, 0x7f, 0x0, 0x0, 0x7f,
    0xff, 0x0, 0x0, 0xff, 0xff, 0x0, 0x0, 0xff,
    0x80, 0xff, 0xff, 0x7f,

    /* U+0031 "1" */
    0x40, 0xff, 0xff, 0x7f, 0xff, 0xff, 0x3f,

    /* U+0032 "2" */
    0x3f, 0xff, 0xff, 0x80, 0x0, 0x0, 0x0, 0xff,
    0x0, 0x0, 0x0, 0xff, 0x7f, 0xff, 0xff, 0x7f,
    0xff, 0x0, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0,
    0x80, 0xff, 0xff, 0x40,

    /* U+0033 "3" */
    0x3f, 0xff, 0xff, 0x80, 0x0, 0x0, 0x0, 0xff,
    0x0, 0x0, 0x0, 0xff, 0x3f, 0xff, 0xff, 0xbf,
    0x0, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0xff,
    0x3f, 0xff, 0xff, 0x7f,

    /* U+0034 "4" */
    0x40, 0x0, 0x0, 0x40, 0xff, 0x0, 0x0, 0xff,
    0xff, 0x0, 0x0, 0xff, 0x80, 0xff, 0xff, 0xbf,
    0x0, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0xff,
    0x0, 0x0, 0x0, 0x3f,

    /* U+0035 "5" */
    0x7f, 0xff, 0xff, 0x40, 0xff, 0x0, 0x0, 0x0,
    0xff, 0x0, 0x0, 0x0, 0x80, 0xff, 0xff, 0x80,
    0x0, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0xff,
    0x3f, 0xff, 0xff, 0x7f,

    /* U+0036 "6" */
    0x7f, 0xff, 0xff, 0x40, 0xff, 0x0, 0x0, 0x0,
    0xff, 0x0, 0x0, 0x0, 0xc0, 0xff, 0xff, 0x80,
    0xff, 0x0, 0x0, 0xff, 0xff, 0x0, 0x0, 0xff,
    0x80, 0xff, 0xff, 0x7f,

    /* U+0037 "7" */
    0x3f, 0xff, 0xff, 0x80, 0x0, 0x0, 0x0, 0xff,
    0x0, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0x7f,
    0x0, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0xff,
    0x0, 0x0, 0x0, 0x3f,

    /* U+0038 "8" */
    0x7f, 0xff, 0xff, 0x80, 0xff, 0x0, 0x0, 0xff,
    0xff, 0x0, 0x0, 0xff, 0xc0, 0xff, 0xff, 0xbf,
    0xff, 0x0, 0x0, 0xff, 0xff, 0x0, 0x0, 0xff,
    0x80, 0xff, 0xff, 0x7f,

    /* U+0039 "9" */
    0x7f, 0xff, 0xff, 0x80, 0xff, 0x0, 0x0, 0xff,
    0xff, 0x0, 0x0, 0xff, 0x80, 0xff, 0xff, 0xbf,
    0x0, 0x0, 0x0, 0xff, 0x0, 0x0, 0x0, 0xff,
    0x3f, 0xff, 0xff, 0x7f,

    /* U+003A ":" */
    0xb8, 0x0, 0x0, 0x0, 0xb8,

    /* U+005F "_" */
    0x3f, 0xff, 0xff, 0xff, 0x40,

    /* U+0060 "`" */
    0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 32, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 96, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 5, .adv_w = 32, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 6, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 34, .adv_w = 32, .box_w = 1, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 41, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 69, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 97, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 125, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 153, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 181, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 209, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 237, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 265, .adv_w = 32, .box_w = 1, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 270, .adv_w = 80, .box_w = 5, .box_h = 1, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 275, .adv_w = 80, .box_w = 1, .box_h = 1, .ofs_x = 3, .ofs_y = 5}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0xd, 0xe, 0x10, 0x11, 0x12, 0x13, 0x14,
    0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x3f, 0x40
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 65, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 16, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};



/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

#if LVGL_VERSION_MAJOR == 8
/*Store all the custom data of the font*/
static  lv_font_fmt_txt_glyph_cache_t cache;
#endif

#if LVGL_VERSION_MAJOR >= 8
static const lv_font_fmt_txt_dsc_t font_dsc = {
#else
static lv_font_fmt_txt_dsc_t font_dsc = {
#endif
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,
    .bpp = 8,
    .kern_classes = 0,
    .bitmap_format = 0,
#if LVGL_VERSION_MAJOR == 8
    .cache = &cache
#endif
};



/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
#if LVGL_VERSION_MAJOR >= 8
const lv_font_t seven_segment = {
#else
lv_font_t seven_segment = {
#endif
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 8,          /*The maximum line height required by the font*/
    .base_line = 1,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if SEVEN_SEGMENT*/

