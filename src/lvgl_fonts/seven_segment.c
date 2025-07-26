/*******************************************************************************
 * Size: 7 px
 * Bpp: 8
 * Glyphs: -0123456789
 * jethomson: Created by hand. Since transparency is used this cannot be 1 bpp since some values cannot be represented by just 0 or 1.
 ******************************************************************************/

#ifdef __has_include
    #if __has_include("lvgl.h")
        #ifndef LV_LVGL_H_INCLUDE_SIMPLE
            #define LV_LVGL_H_INCLUDE_SIMPLE
        #endif
    #endif
#endif

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
    0x80, 0xff, 0xff, 0xff, 0x80,

    /* U+0030 "0" */
    0x80, 0xff, 0xff, 0x80,
    0xff, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0x80, 0x00, 0x00, 0x80,
    0xff, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0x80, 0xff, 0xff, 0x80,

    /* U+0031 "1" */
    0x80,
    0xff,
    0xff,
    0x80,
    0xff,
    0xff,
    0x80,

    /* U+0032 "2" */
    0x80, 0xff, 0xff, 0x80,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0x80, 0xff, 0xff, 0x80,
    0xff, 0x00, 0x00, 0x00,
    0xff, 0x00, 0x00, 0x00,
    0x80, 0xff, 0xff, 0x80,

    /* U+0033 "3" */
    0x80, 0xff, 0xff, 0x80,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0x80, 0xff, 0xff, 0x80,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0x80, 0xff, 0xff, 0x80,

    /* U+0034 "4" */
    0x80, 0x00, 0x00, 0x80,
    0xff, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0x80, 0xff, 0xff, 0x80,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0x80,

    /* U+0035 "5" */
    0x80, 0xff, 0xff, 0x80,
    0xff, 0x00, 0x00, 0x00,
    0xff, 0x00, 0x00, 0x00,
    0x80, 0xff, 0xff, 0x80,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0x80, 0xff, 0xff, 0x80,

    /* U+0036 "6" */
    0x80, 0xff, 0xff, 0x80,
    0xff, 0x00, 0x00, 0x00,
    0xff, 0x00, 0x00, 0x00,
    0x80, 0xff, 0xff, 0x80,
    0xff, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0x80, 0xff, 0xff, 0x80,

    /* U+0037 "7" */
    0x80, 0xff, 0xff, 0x80,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0x80,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0x80,

    /* U+0038 "8" */
    0x80, 0xff, 0xff, 0x80,
    0xff, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0x80, 0xff, 0xff, 0x80,
    0xff, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0x80, 0xff, 0xff, 0x80,

    /* U+0039 "9" */
    0x80, 0xff, 0xff, 0x80,
    0xff, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0xff,
    0x80, 0xff, 0xff, 0x80,
    0x00, 0x00, 0x00, 0xff,
    0x00, 0x00, 0x00, 0xff,
    0x80, 0xff, 0xff, 0x80
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 32, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 0, .adv_w = 96, .box_w = 5, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 5, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 33, .adv_w = 32, .box_w = 1, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 40, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 68, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 96, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 124, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 152, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 180, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 208, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 236, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_0[] = {
    0x0, 0xd
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 14, .glyph_id_start = 1,
        .unicode_list = unicode_list_0, .glyph_id_ofs_list = NULL, .list_length = 2, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    },
    {
        .range_start = 48, .range_length = 10, .glyph_id_start = 3,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
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
    .cmap_num = 2,
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
    .line_height = 7,          /*The maximum line height required by the font*/
    .base_line = 0,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = 0,
    .underline_thickness = 0,
#endif
    .static_bitmap = 0,
    .dsc = &font_dsc,          /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
#if LV_VERSION_CHECK(8, 2, 0) || LVGL_VERSION_MAJOR >= 9
    .fallback = NULL,
#endif
    .user_data = NULL,
};



#endif /*#if SEVEN_SEGMENT*/
