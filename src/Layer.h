/*
  This code is copyright 2024 Jonathan Thomson, jethomson.wordpress.com

  Permission to use, copy, modify, and distribute this software
  and its documentation for any purpose and without fee is hereby
  granted, provided that the above copyright notice appear in all
  copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

#ifndef LAYERS_H
#define LAYERS_H

#include "project.h"
#include "ReAnimator.h"
#include <LittleFS.h>
#include "ArduinoJson-v6.h"

#include "lv_font.h"

#define MD 16 // width or height of matrix in number of LEDs

extern lv_font_t ascii_sector;
extern lv_font_t seven_segment;


class Layer {
    enum LayerType {Pattern_t = 0, Accent_t = 1, Image_t = 2, Text_t = 3, Info_t = 4};
    //enum Direction {STILL = 0, N = 1, NE = 2, E = 3, SE = 4, S = 5, SW = 6, W = 7, NW = 8};

    //LayerType _ltype = static_cast<LayerType>(-1);
    //int8_t _id = -1;
    LayerType _ltype;
    int8_t _id;
    CRGBA leds[NUM_LEDS];
    //CRGBA tleds[NUM_LEDS]; // for use with ntranslate()
    ReAnimator* GlowSerum = nullptr;
    //ReAnimator* MiskaTonic;
    
    Pattern pattern;
    
    // the frontend color picker is focused on RGB so try to honor the RGB color picked.
    // sometimes it makes more sense to work with a hue, so we have a hue variable that is derived from rgb.
    // color can be determined externally (*rgb) or internally (internal_rgb) to the layer.
    // if internal_rgb is user rgb points to that.
    CRGB* rgb = nullptr;
    uint8_t hue;
    CRGB internal_rgb;

    struct Point {
      uint8_t x;
      uint8_t y;
    };

    uint8_t direction = 0;
    bool t_initial = true;
    bool t_visible = false;
    bool t_has_entered = false;
    int8_t dx = 0;
    int8_t dy = 0;


    lv_font_t* font = &ascii_sector;
    lv_font_t* clkfont = &seven_segment;

    struct fstring {
      String s;
      int8_t vmargin = 0; // for centering text vertically
      uint8_t tracking = 1; // spacing between letters
    } ftext;

    uint32_t st_pm = 0;

    uint32_t mts_pm = 0;
    uint16_t mts_i = 0;

    uint8_t mcs_column = 0;
    uint8_t mcs_tracking = 0; // spacing between letters

  public:

    Layer();
    ~Layer();

    void setup(LayerType ltype, int8_t id = -1);
    void set_color(CRGB* color);
    void set_color(CRGB color);
    void set_direction(uint8_t d);
    void clear();
    CRGBA get_pixel(uint16_t i);
    void refresh();

    bool colorFromHexString(byte* rgb, const char* in);
    bool deserializeSegment(JsonObject root, CRGBA leds[], uint16_t leds_len);
    //bool load_image_from_json(String json, String* message = nullptr);
    bool load_image_from_file(String fs_path, String* message = nullptr);

    void pac_man_cb(uint8_t event);
    void noop_cb(uint8_t event);
    void set_plfx(uint8_t id);
    void set_alfx(uint8_t id);

    Point serp2cart(uint8_t i);
    int16_t cart2serp(Point p);
    void flip(CRGB sm[NUM_LEDS], bool dim);
    uint16_t translate(uint16_t i, int8_t xi, int8_t yi, int8_t sx, int8_t sy, bool wrap, int8_t gap);
    void ntranslate(CRGBA in[NUM_LEDS], CRGBA out[NUM_LEDS], int8_t xi = 0, int8_t yi = 0, int8_t sx = 1, int8_t sy = 1, bool wrap = true, int8_t gap = 0);
    uint16_t director(uint16_t i);

    uint8_t get_text_center(String s);
    void matrix_char(char c);
    bool matrix_char_shift(char c, int8_t vmargin = 0);
    void matrix_text(String s);
    //void matrix_text_shift(struct fstring ftext);
    void matrix_text_shift();
    void set_text(String s);

    void set_nlfx(uint8_t id);
    void show_date_time();
    void setup_clock();


  private:


// ++++++++++++++++++++++++++++++
// ++++++++++ HELPERS +++++++++++
// ++++++++++++++++++++++++++++++


};

#endif
