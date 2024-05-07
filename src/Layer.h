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
#include "minjson.h"

#include "lv_font.h"

#define MD 16 // width or height of matrix in number of LEDs

#define TRANSPARENT (uint32_t)0x424242



// pl - pattern layer. currently background. could possibly dynamically change the z height of where a pattern is drawn such that it is sometimes above an image, text, etc.
// il - image layer. pixel art is written here. if the image is completely transparent then only effects will be shown.
// al - accent layer. currently accent effects are implemented for the overlay, but they could possibly be put underneath. for example, an image could sparkle from underneath. twinkling stars?
// tl - text layer. not used yet.

extern lv_font_t ascii_sector;

class Layer {
    enum LayerType {Pattern_t = 0, Accent_t = 1, Image_t = 2, Text_t = 3};
    enum Direction {RTL = 0, LTR = 1, DOWN = 2, UP = 3};

    LayerType _ltype;
    CRGBA leds[NUM_LEDS];
    ReAnimator* GlowSerum = nullptr;
    //ReAnimator* MiskaTonic;
    
    Pattern pattern;
    
    // the frontend color picker is focused on RGB so try to honor the RGB color picked.
    // sometimes it makes more sense to work with a hue, so we have a hue variable that is derived from rgb.
    // instead of rgb being determined somewhere else (hence the pointer) the layer's color is explicitly set
    // by saving a color to internal_rgb and point rgb to that.
    CRGB* rgb = nullptr;
    uint8_t hue;
    CRGB internal_rgb;

    lv_font_t* font = &ascii_sector;

    struct fstring {
      String s;
      uint8_t vmargin = 0;
      uint8_t tracking = 1;
    } ftext;


    struct Point {
      uint8_t x;
      uint8_t y;
    };

  public:

    Layer();
    ~Layer();

    void set_color(CRGB color);
    void set_color(CRGB* color);
    void run();
    void clear();
    void set_type(LayerType ltype);
    CRGBA get_pixel(uint16_t i);
    bool load_image_from_json(String json, String* message = nullptr);
    bool load_image_from_file(String fs_path, String* message = nullptr);
    void pac_man_cb(uint8_t event);
    void noop_cb(uint8_t event);
    void set_alfx(uint8_t alfx);
    void set_plfx(uint8_t plfx);
    Point serp2cart(uint8_t i);
    int16_t cart2serp(Point p);
    void flip(CRGB sm[NUM_LEDS], bool dim);
    void move(CRGB sm[NUM_LEDS], bool dim, uint8_t d);
    void shift(CRGB in[NUM_LEDS], CRGB out[NUM_LEDS], Direction t=RTL, bool loop=false, uint8_t gap=0);
    void position_OLD(CRGB in[NUM_LEDS], CRGB out[NUM_LEDS], int8_t x0 = 0, int8_t y0 = 0, bool wrap=true, uint8_t gap=0);
    void position(CRGB in[NUM_LEDS], CRGB out[NUM_LEDS], int8_t xi = 0, int8_t yi = 0, int8_t sx = 1, int8_t sy = 1, bool wrap = true, int8_t gap = 0);
    void posmove(CRGB in[NUM_LEDS], CRGB out[NUM_LEDS], int8_t xi = 0, int8_t yi = 0, int8_t sx = 1, int8_t sy = 1, bool wrap=true, uint8_t gap=0);
    uint8_t get_text_height_old(String s);
    uint8_t get_text_height(String s);
    void matrix_char(char c);
    bool matrix_char_shift(char c, uint8_t vmargin = 0);
    void matrix_text(String s);
    //void matrix_text_shift(struct fstring ftext);
    void matrix_text_shift();
    void set_text(String s);

  private:


// ++++++++++++++++++++++++++++++
// ++++++++++ HELPERS +++++++++++
// ++++++++++++++++++++++++++++++


};


#endif
