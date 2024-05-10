#include "lv_font.h"

//https://lvgl.io/tools/font_conv_v5_3
//https://fontstruct.com/fontstructions/show/1697974/7-segments-6

#define USE_SEVEN_SEGMENT 8

#if USE_SEVEN_SEGMENT != 0	/*Can be enabled in lv_conf.h*/

/***********************************************************************************
 * 7-segments.ttf 8 px Font in U+0030 (0) .. U+0039 (9)  range with all bpp
***********************************************************************************/

/*Store the image of the letters (glyph)*/
static const uint8_t seven_segment_glyph_bitmap[] = 
{
#if USE_SEVEN_SEGMENT == 1
  /*Unicode: U+0030 (0) , Width: 4 */
  0x00,  //.... 
  0x60,  //.%%. 
  0x90,  //%..% 
  0x90,  //%..% 
  0x00,  //.... 
  0x90,  //%..% 
  0x90,  //%..% 
  0x60,  //.%%. 


  /*Unicode: U+0031 (1) , Width: 1 */
  0x00,  //. 
  0x00,  //. 
  0x80,  //% 
  0x80,  //% 
  0x00,  //. 
  0x80,  //% 
  0x80,  //% 
  0x00,  //. 


  /*Unicode: U+0032 (2) , Width: 4 */
  0x00,  //.... 
  0x60,  //.%%. 
  0x10,  //...% 
  0x10,  //...% 
  0x60,  //.%%. 
  0x80,  //%... 
  0x80,  //%... 
  0x60,  //.%%. 


  /*Unicode: U+0033 (3) , Width: 4 */
  0x00,  //.... 
  0x60,  //.%%. 
  0x10,  //...% 
  0x10,  //...% 
  0x70,  //.%%% 
  0x10,  //...% 
  0x10,  //...% 
  0x60,  //.%%. 


  /*Unicode: U+0034 (4) , Width: 4 */
  0x00,  //.... 
  0x00,  //.... 
  0x90,  //%..% 
  0x90,  //%..% 
  0x70,  //.%%% 
  0x10,  //...% 
  0x10,  //...% 
  0x00,  //.... 


  /*Unicode: U+0035 (5) , Width: 4 */
  0x00,  //.... 
  0x60,  //.%%. 
  0x80,  //%... 
  0x80,  //%... 
  0x60,  //.%%. 
  0x10,  //...% 
  0x10,  //...% 
  0x60,  //.%%. 


  /*Unicode: U+0036 (6) , Width: 4 */
  0x00,  //.... 
  0x60,  //.%%. 
  0x80,  //%... 
  0x80,  //%... 
  0xe0,  //%%%. 
  0x90,  //%..% 
  0x90,  //%..% 
  0x60,  //.%%. 


  /*Unicode: U+0037 (7) , Width: 4 */
  0x00,  //.... 
  0x60,  //.%%. 
  0x10,  //...% 
  0x10,  //...% 
  0x00,  //.... 
  0x10,  //...% 
  0x10,  //...% 
  0x00,  //.... 


  /*Unicode: U+0038 (8) , Width: 4 */
  0x00,  //.... 
  0x60,  //.%%. 
  0x90,  //%..% 
  0x90,  //%..% 
  0xf0,  //%%%% 
  0x90,  //%..% 
  0x90,  //%..% 
  0x60,  //.%%. 


  /*Unicode: U+0039 (9) , Width: 4 */
  0x00,  //.... 
  0x60,  //.%%. 
  0x90,  //%..% 
  0x90,  //%..% 
  0x70,  //.%%% 
  0x10,  //...% 
  0x10,  //...% 
  0x60,  //.%%. 



#elif USE_SEVEN_SEGMENT == 2
  /*Unicode: U+0030 (0) , Width: 4 */
  0x00,  //.... 
  0x7d,  //+@@+ 
  0xc3,  //@..@ 
  0xc3,  //@..@ 
  0x41,  //+..+ 
  0xc3,  //@..@ 
  0xc3,  //@..@ 
  0x7d,  //+@@+ 


  /*Unicode: U+0031 (1) , Width: 1 */
  0x00,  //. 
  0x00,  //. 
  0xc0,  //@ 
  0xc0,  //@ 
  0x40,  //+ 
  0xc0,  //@ 
  0xc0,  //@ 
  0x00,  //. 


  /*Unicode: U+0032 (2) , Width: 4 */
  0x00,  //.... 
  0x3d,  //.@@+ 
  0x03,  //...@ 
  0x03,  //...@ 
  0x7d,  //+@@+ 
  0xc0,  //@... 
  0xc0,  //@... 
  0x7c,  //+@@. 


  /*Unicode: U+0033 (3) , Width: 4 */
  0x00,  //.... 
  0x3d,  //.@@+ 
  0x03,  //...@ 
  0x03,  //...@ 
  0x3e,  //.@@% 
  0x03,  //...@ 
  0x03,  //...@ 
  0x3d,  //.@@+ 


  /*Unicode: U+0034 (4) , Width: 4 */
  0x00,  //.... 
  0x00,  //.... 
  0xc3,  //@..@ 
  0xc3,  //@..@ 
  0x7e,  //+@@% 
  0x03,  //...@ 
  0x03,  //...@ 
  0x00,  //.... 


  /*Unicode: U+0035 (5) , Width: 4 */
  0x00,  //.... 
  0x7c,  //+@@. 
  0xc0,  //@... 
  0xc0,  //@... 
  0x7d,  //+@@+ 
  0x03,  //...@ 
  0x03,  //...@ 
  0x3d,  //.@@+ 


  /*Unicode: U+0036 (6) , Width: 4 */
  0x00,  //.... 
  0x7c,  //+@@. 
  0xc0,  //@... 
  0xc0,  //@... 
  0xbd,  //%@@+ 
  0xc3,  //@..@ 
  0xc3,  //@..@ 
  0x7d,  //+@@+ 


  /*Unicode: U+0037 (7) , Width: 4 */
  0x00,  //.... 
  0x3d,  //.@@+ 
  0x03,  //...@ 
  0x03,  //...@ 
  0x01,  //...+ 
  0x03,  //...@ 
  0x03,  //...@ 
  0x00,  //.... 


  /*Unicode: U+0038 (8) , Width: 4 */
  0x00,  //.... 
  0x7d,  //+@@+ 
  0xc3,  //@..@ 
  0xc3,  //@..@ 
  0xbe,  //%@@% 
  0xc3,  //@..@ 
  0xc3,  //@..@ 
  0x7d,  //+@@+ 


  /*Unicode: U+0039 (9) , Width: 4 */
  0x00,  //.... 
  0x7d,  //+@@+ 
  0xc3,  //@..@ 
  0xc3,  //@..@ 
  0x7e,  //+@@% 
  0x03,  //...@ 
  0x03,  //...@ 
  0x3d,  //.@@+ 



#elif USE_SEVEN_SEGMENT == 4
  /*Unicode: U+0030 (0) , Width: 4 */
  0x00, 0x00,  //.... 
  0x7f, 0xf7,  //+@@+ 
  0xf0, 0x0f,  //@..@ 
  0xf0, 0x0f,  //@..@ 
  0x70, 0x07,  //+..+ 
  0xf0, 0x0f,  //@..@ 
  0xf0, 0x0f,  //@..@ 
  0x7f, 0xf7,  //+@@+ 


  /*Unicode: U+0031 (1) , Width: 1 */
  0x00,  //. 
  0x30,  //. 
  0xf0,  //@ 
  0xf0,  //@ 
  0x70,  //+ 
  0xf0,  //@ 
  0xf0,  //@ 
  0x30,  //. 


  /*Unicode: U+0032 (2) , Width: 4 */
  0x00, 0x00,  //.... 
  0x3f, 0xf7,  //.@@+ 
  0x00, 0x0f,  //...@ 
  0x00, 0x0f,  //...@ 
  0x7f, 0xf7,  //+@@+ 
  0xf0, 0x00,  //@... 
  0xf0, 0x00,  //@... 
  0x7f, 0xf3,  //+@@. 


  /*Unicode: U+0033 (3) , Width: 4 */
  0x00, 0x00,  //.... 
  0x3f, 0xf7,  //.@@+ 
  0x00, 0x0f,  //...@ 
  0x00, 0x0f,  //...@ 
  0x3f, 0xfb,  //.@@% 
  0x00, 0x0f,  //...@ 
  0x00, 0x0f,  //...@ 
  0x3f, 0xf7,  //.@@+ 


  /*Unicode: U+0034 (4) , Width: 4 */
  0x00, 0x00,  //.... 
  0x30, 0x03,  //.... 
  0xf0, 0x0f,  //@..@ 
  0xf0, 0x0f,  //@..@ 
  0x7f, 0xfb,  //+@@% 
  0x00, 0x0f,  //...@ 
  0x00, 0x0f,  //...@ 
  0x00, 0x03,  //.... 


  /*Unicode: U+0035 (5) , Width: 4 */
  0x00, 0x00,  //.... 
  0x7f, 0xf3,  //+@@. 
  0xf0, 0x00,  //@... 
  0xf0, 0x00,  //@... 
  0x7f, 0xf7,  //+@@+ 
  0x00, 0x0f,  //...@ 
  0x00, 0x0f,  //...@ 
  0x3f, 0xf7,  //.@@+ 


  /*Unicode: U+0036 (6) , Width: 4 */
  0x00, 0x00,  //.... 
  0x7f, 0xf3,  //+@@. 
  0xf0, 0x00,  //@... 
  0xf0, 0x00,  //@... 
  0xbf, 0xf7,  //%@@+ 
  0xf0, 0x0f,  //@..@ 
  0xf0, 0x0f,  //@..@ 
  0x7f, 0xf7,  //+@@+ 


  /*Unicode: U+0037 (7) , Width: 4 */
  0x00, 0x00,  //.... 
  0x3f, 0xf7,  //.@@+ 
  0x00, 0x0f,  //...@ 
  0x00, 0x0f,  //...@ 
  0x00, 0x07,  //...+ 
  0x00, 0x0f,  //...@ 
  0x00, 0x0f,  //...@ 
  0x00, 0x03,  //.... 


  /*Unicode: U+0038 (8) , Width: 4 */
  0x00, 0x00,  //.... 
  0x7f, 0xf7,  //+@@+ 
  0xf0, 0x0f,  //@..@ 
  0xf0, 0x0f,  //@..@ 
  0xbf, 0xfb,  //%@@% 
  0xf0, 0x0f,  //@..@ 
  0xf0, 0x0f,  //@..@ 
  0x7f, 0xf7,  //+@@+ 


  /*Unicode: U+0039 (9) , Width: 4 */
  0x00, 0x00,  //.... 
  0x7f, 0xf7,  //+@@+ 
  0xf0, 0x0f,  //@..@ 
  0xf0, 0x0f,  //@..@ 
  0x7f, 0xfb,  //+@@% 
  0x00, 0x0f,  //...@ 
  0x00, 0x0f,  //...@ 
  0x3f, 0xf7,  //.@@+ 



#elif USE_SEVEN_SEGMENT == 8
  /*Unicode: U+0030 (0) , Width: 4 */
  0x00, 0x00, 0x00, 0x00,  //.... 
  0xbb, 0xff, 0xff, 0xbb,  //+@@+ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xbb, 0x00, 0x00, 0xbb,  //+..+ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xbb, 0xff, 0xff, 0xbb,  //+@@+ 


  /*Unicode: U+0031 (1) , Width: 4 */
  0x00, 0x00, 0x00, 0x00,  //.... 
  0x00, 0x00, 0x00, 0xbb,  //.... 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xbb,  //...+ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xbb,  //.... 

  /*Unicode: U+0032 (2) , Width: 4 */
  0x00, 0x00, 0x00, 0x00,  //.... 
  0xbb, 0xff, 0xff, 0xbb,  //.@@+ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0xbb, 0xff, 0xff, 0xbb,  //+@@+ 
  0xff, 0x00, 0x00, 0x00,  //@... 
  0xff, 0x00, 0x00, 0x00,  //@... 
  0xbb, 0xff, 0xff, 0xbb,  //+@@. 


  /*Unicode: U+0033 (3) , Width: 4 */
  0x00, 0x00, 0x00, 0x00,  //.... 
  0xbb, 0xff, 0xff, 0xbb,  //.@@+ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0xbb, 0xff, 0xff, 0xbb,  //.@@+ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0xbb, 0xff, 0xff, 0xbb,  //.@@+ 


  /*Unicode: U+0034 (4) , Width: 4 */
  0x00, 0x00, 0x00, 0x00,  //.... 
  0xbb, 0x00, 0x00, 0xbb,  //.... 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xbb, 0xff, 0xff, 0xbb,  //+@@+ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xbb,  //.... 


  /*Unicode: U+0035 (5) , Width: 4 */
  0x00, 0x00, 0x00, 0x00,  //.... 
  0xbb, 0xff, 0xff, 0xbb,  //+@@. 
  0xff, 0x00, 0x00, 0x00,  //@... 
  0xff, 0x00, 0x00, 0x00,  //@... 
  0xbb, 0xff, 0xff, 0xbb,  //+@@+ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0xbb, 0xff, 0xff, 0xbb,  //.@@+ 


  /*Unicode: U+0036 (6) , Width: 4 */
  0x00, 0x00, 0x00, 0x00,  //.... 
  0xbb, 0xff, 0xff, 0xbb,  //+@@. 
  0xff, 0x00, 0x00, 0x00,  //@... 
  0xff, 0x00, 0x00, 0x00,  //@... 
  0xbb, 0xff, 0xff, 0xbb,  //+@@+ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xbb, 0xff, 0xff, 0xbb,  //+@@+ 


  /*Unicode: U+0037 (7) , Width: 4 */
  0x00, 0x00, 0x00, 0x00,  //.... 
  0xbb, 0xff, 0xff, 0xbb,  //.@@+ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xbb,  //...+ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xbb,  //.... 


  /*Unicode: U+0038 (8) , Width: 4 */
  0x00, 0x00, 0x00, 0x00,  //.... 
  0xbb, 0xff, 0xff, 0xbb,  //+@@+ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xbb, 0xff, 0xff, 0xbb,  //+@@+ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xbb, 0xff, 0xff, 0xbb,  //+@@+ 


  /*Unicode: U+0039 (9) , Width: 4 */
  0x00, 0x00, 0x00, 0x00,  //.... 
  0xbb, 0xff, 0xff, 0xbb,  //+@@+ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xff, 0x00, 0x00, 0xff,  //@..@ 
  0xbb, 0xff, 0xff, 0xbb,  //+@@+ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0x00, 0x00, 0x00, 0xff,  //...@ 
  0xbb, 0xff, 0xff, 0xbb,  //.@@+ 

#endif
};


/*Store the glyph descriptions*/
static const lv_font_glyph_dsc_t seven_segment_glyph_dsc[] = 
{
#if USE_SEVEN_SEGMENT == 1
  {.w_px = 4,	.glyph_index = 0},	/*Unicode: U+0030 (0)*/
  {.w_px = 1,	.glyph_index = 8},	/*Unicode: U+0031 (1)*/
  {.w_px = 4,	.glyph_index = 16},	/*Unicode: U+0032 (2)*/
  {.w_px = 4,	.glyph_index = 24},	/*Unicode: U+0033 (3)*/
  {.w_px = 4,	.glyph_index = 32},	/*Unicode: U+0034 (4)*/
  {.w_px = 4,	.glyph_index = 40},	/*Unicode: U+0035 (5)*/
  {.w_px = 4,	.glyph_index = 48},	/*Unicode: U+0036 (6)*/
  {.w_px = 4,	.glyph_index = 56},	/*Unicode: U+0037 (7)*/
  {.w_px = 4,	.glyph_index = 64},	/*Unicode: U+0038 (8)*/
  {.w_px = 4,	.glyph_index = 72},	/*Unicode: U+0039 (9)*/

#elif USE_SEVEN_SEGMENT == 2
  {.w_px = 4,	.glyph_index = 0},	/*Unicode: U+0030 (0)*/
  {.w_px = 1,	.glyph_index = 8},	/*Unicode: U+0031 (1)*/
  {.w_px = 4,	.glyph_index = 16},	/*Unicode: U+0032 (2)*/
  {.w_px = 4,	.glyph_index = 24},	/*Unicode: U+0033 (3)*/
  {.w_px = 4,	.glyph_index = 32},	/*Unicode: U+0034 (4)*/
  {.w_px = 4,	.glyph_index = 40},	/*Unicode: U+0035 (5)*/
  {.w_px = 4,	.glyph_index = 48},	/*Unicode: U+0036 (6)*/
  {.w_px = 4,	.glyph_index = 56},	/*Unicode: U+0037 (7)*/
  {.w_px = 4,	.glyph_index = 64},	/*Unicode: U+0038 (8)*/
  {.w_px = 4,	.glyph_index = 72},	/*Unicode: U+0039 (9)*/

#elif USE_SEVEN_SEGMENT == 4
  {.w_px = 4,	.glyph_index = 0},	/*Unicode: U+0030 (0)*/
  {.w_px = 1,	.glyph_index = 16},	/*Unicode: U+0031 (1)*/
  {.w_px = 4,	.glyph_index = 24},	/*Unicode: U+0032 (2)*/
  {.w_px = 4,	.glyph_index = 40},	/*Unicode: U+0033 (3)*/
  {.w_px = 4,	.glyph_index = 56},	/*Unicode: U+0034 (4)*/
  {.w_px = 4,	.glyph_index = 72},	/*Unicode: U+0035 (5)*/
  {.w_px = 4,	.glyph_index = 88},	/*Unicode: U+0036 (6)*/
  {.w_px = 4,	.glyph_index = 104},	/*Unicode: U+0037 (7)*/
  {.w_px = 4,	.glyph_index = 120},	/*Unicode: U+0038 (8)*/
  {.w_px = 4,	.glyph_index = 136},	/*Unicode: U+0039 (9)*/

#elif USE_SEVEN_SEGMENT == 8
  {.w_px = 4,	.glyph_index = 0},	/*Unicode: U+0030 (0)*/
  {.w_px = 4,	.glyph_index = 32},	/*Unicode: U+0031 (1)*/
  {.w_px = 4,	.glyph_index = 64},	/*Unicode: U+0032 (2)*/
  {.w_px = 4,	.glyph_index = 96},	/*Unicode: U+0033 (3)*/
  {.w_px = 4,	.glyph_index = 128},	/*Unicode: U+0034 (4)*/
  {.w_px = 4,	.glyph_index = 160},	/*Unicode: U+0035 (5)*/
  {.w_px = 4,	.glyph_index = 192},	/*Unicode: U+0035 (6)*/
  {.w_px = 4,	.glyph_index = 224},	/*Unicode: U+0036 (7)*/
  {.w_px = 4,	.glyph_index = 256},	/*Unicode: U+0037 (8)*/
  {.w_px = 4,	.glyph_index = 288},	/*Unicode: U+0038 (9)*/

#endif
};

lv_font_t seven_segment = 
{
    .unicode_first = 48,	/*First Unicode letter in this font*/
    .unicode_last = 57,	/*Last Unicode letter in this font*/
    .h_px = 8,				/*Font height in pixels*/
    .glyph_bitmap = seven_segment_glyph_bitmap,	/*Bitmap of glyphs*/
    .glyph_dsc = seven_segment_glyph_dsc,		/*Description of glyphs*/
    .glyph_cnt = 10,			/*Number of glyphs in the font*/
    .unicode_list = NULL,	/*Every character in the font from 'unicode_first' to 'unicode_last'*/
    .get_bitmap = lv_font_get_bitmap_continuous,	/*Function pointer to get glyph's bitmap*/
    .get_width = lv_font_get_width_continuous,	/*Function pointer to get glyph's width*/
#if USE_SEVEN_SEGMENT == 1
    .bpp = 1,				/*Bit per pixel*/
 #elif USE_SEVEN_SEGMENT == 2
    .bpp = 2,				/*Bit per pixel*/
 #elif USE_SEVEN_SEGMENT == 4
    .bpp = 4,				/*Bit per pixel*/
 #elif USE_SEVEN_SEGMENT == 8
    .bpp = 8,				/*Bit per pixel*/
#endif
    .monospace = 0,				/*Fix width (0: if not used)*/
    .next_page = NULL,		/*Pointer to a font extension*/
};

#endif /*USE_SEVEN_SEGMENT*/

