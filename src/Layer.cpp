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

#include "Layer.h"


//color mangling macros from WLED wled.h
//modified slightly for alpha channel instead of white channel
#ifndef RGBA32
#define RGBA32(r,g,b,a) (uint32_t((byte(a) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))
#endif


Layer::Layer(){
  // ??? better to pass a reference to LittleFS from main?
  if (!LittleFS.begin()) {
    DEBUG_PRINTLN("LittleFS initialisation failed!");
    while (1) yield(); // cannot proceed without filesystem
  }

  clear();
}


Layer::~Layer() {
  if (GlowSerum != nullptr) {
    delete GlowSerum;
    GlowSerum = nullptr;
  }
}


void Layer::set_color(CRGB color) {
  internal_rgb = color;
  rgb = &internal_rgb;
  CHSV chsv = rgb2hsv_approximate(color);
  hue = chsv.h; // !!BUG!! if color is 0x000000 (black) then hue will be 0 which is red when CHSV(hue, 255, 255)
}


void Layer::set_color(CRGB* color) {
  rgb = color;
  CHSV chsv = rgb2hsv_approximate(*color);
  hue = chsv.h;
}


void Layer::set_type(LayerType ltype) {
  if (ltype == Pattern_t || ltype == Accent_t) {
    if (GlowSerum != nullptr) {
      delete GlowSerum;
      GlowSerum = nullptr;
    }
    // cast CRGBA array to CRGB wastes 1 byte * NUM_LEDS. might update ReAnimator to work with CRGBA one day.
    GlowSerum = new ReAnimator((CRGB *)&leds[0], rgb, LED_STRIP_MILLIAMPS);
    
    GlowSerum->set_pattern(NONE);
    GlowSerum->set_overlay(NO_OVERLAY, false);
    //GlowSerum->set_cb(&Layer::noop_cb);

    GlowSerum->set_autocycle_interval(10000);
    GlowSerum->set_autocycle_enabled(false);
    GlowSerum->set_flipflop_interval(6000);
    GlowSerum->set_flipflop_enabled(false);
  }
  
  _ltype = ltype;
}


void Layer::set_direction(uint8_t d) {
  initial = true;
  direction = d;
}



void Layer::clear() {
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    //leds[i] = CRGB::Black;
    leds[i] = 0x00000000;
  }
}

CRGBA Layer::get_pixel(uint16_t i) {

  //CRGBA pixel_out = 0xFF000000; // if black with no transparency is used it creates a sort of spotlight effect
  CRGBA pixel_out = 0x00000000;

  if (_ltype == Image_t || _ltype == Text_t) {
    uint16_t ti = director(i);
    if (0 <= ti && ti < NUM_LEDS) {
      pixel_out = leds[ti];
    }
  }
  else if (_ltype == Pattern_t || _ltype == Accent_t) {
    uint16_t ti = director(i);
    if (0 <= ti && ti < NUM_LEDS) {
      // leds[] was cast as CRGB when passed to ReAnimator, so the pixels need to be
      // read back out as CRGB
      pixel_out = ((CRGB*)leds)[ti];
      if (pixel_out == 0xFF000000) {
        pixel_out.a = 0;
      }
    }
  }

  return pixel_out;
}


void Layer::run() {
  if (rgb == nullptr) {
    internal_rgb = CRGB::Yellow;
    rgb = &internal_rgb;
  }
  CHSV chsv = rgb2hsv_approximate(*rgb);
  hue = chsv.h; // !!BUG!! if color is 0x000000 (black) then hue will be 0 which is red when CHSV(hue, 255, 255)

  if (GlowSerum != nullptr) {
    // accents only light up a few pixels leaving the rest black
    if (_ltype == Accent_t) {
      clear();
    }
    if (_ltype == Pattern_t || _ltype == Accent_t) {
      GlowSerum->reanimate();
    }
  }


  //if (_ltype == Image_t) {
  //  if ((millis()-pos_pm) > 200) {
  //    pos_pm = millis();
  //    position(leds, tleds, -5, 0, 1, 1, true, -1);
  //  }
  //}

  if (_ltype == Text_t) {
    matrix_text_shift();
  }

}


//****************
// IMAGE FUNCTIONS
//****************
// from WLED colors.cpp
//this uses RRGGBB / RRGGBBWW (RRGGBBAA) order
bool Layer::colorFromHexString(byte* rgb, const char* in) {
  if (in == nullptr) return false;
  size_t inputSize = strnlen(in, 9);
  if (inputSize != 6 && inputSize != 8) return false;

  uint32_t c = strtoul(in, NULL, 16);

  if (inputSize == 6) {
    rgb[0] = (c >> 16);
    rgb[1] = (c >>  8);
    rgb[2] =  c       ;
  } else {
    rgb[0] = (c >> 24);
    rgb[1] = (c >> 16);
    rgb[2] = (c >>  8);
    rgb[3] =  c       ;
  }
  return true;
}


// slightly modified version of deserializeSegment() from WLED json.cpp
bool Layer::deserializeSegment(JsonObject root, CRGBA leds[], uint16_t leds_len)
{
  JsonVariant elem = root["seg"];
  if (elem.is<JsonObject>())
  {
    JsonArray iarr = elem[F("i")]; //set individual LEDs
    if (!iarr.isNull()) {
      uint16_t start = 0, stop = 0;
      byte set = 0; //0 nothing set, 1 start set, 2 range set

      for (size_t i = 0; i < iarr.size(); i++) {
        if(iarr[i].is<JsonInteger>()) {
          if (!set) {
            start = abs(iarr[i].as<int>());
            set++;
          } else {
            stop = abs(iarr[i].as<int>());
            set++;
          }
        } else { //color
          uint8_t rgba[] = {0,0,0,0};
          JsonArray icol = iarr[i];
          if (!icol.isNull()) { //array, e.g. [255,0,0]
            byte sz = icol.size();
            if (sz > 0 && sz < 5) copyArray(icol, rgba);
          } else { //hex string, e.g. "FF0000"
            byte brgba[] = {0,0,0,0};
            const char* hexCol = iarr[i];
            if (colorFromHexString(brgba, hexCol)) {
              for (size_t c = 0; c < 4; c++) rgba[c] = brgba[c];
            }
          }

          if (set < 2 || stop <= start) stop = start + 1;
          // can use FastLED gamma function?
          //uint32_t c = gamma32(RGBA32(rgba[0], rgba[1], rgba[2], rgba[3]));
          uint32_t c = RGBA32(rgba[0], rgba[1], rgba[2], rgba[3]);
          //while (start < stop && start < leds_len) leds[start++] = c;
          while (start < stop) leds[start++] = c;
          set = 0;
        }
      }
    }
  }
  return true;
}

/*
bool Layer::load_image_from_json(String json, String* message) {
  set_type(Image_t);

  bool retval = false;
  //const size_t CAPACITY = JSON_OBJECT_SIZE(6) + JSON_ARRAY_SIZE(360);
  //StaticJsonDocument<CAPACITY> doc;
  DynamicJsonDocument doc(8192);

  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    //Serial.print("deserializeJson() failed: ");
    //Serial.println(error.c_str());
    if (message) {
      *message = F("load_image_from_json: deserializeJson() failed.");
    }
    return false;
  }

  JsonObject object = doc.as<JsonObject>();

  for (uint16_t i = 0; i < NUM_LEDS; i++) leds[i] = 0;
  retval = deserializeSegment(object, leds, NUM_LEDS);

  return retval;
}
*/

bool Layer::load_image_from_file(String fs_path, String* message) {
  set_type(Image_t);

  bool retval = false;
  File file = LittleFS.open(fs_path, "r");
  
  if(!file){
    if (message) {
      *message = F("load_image_from_file(): File not found.");
    }
    return false;
  }

  if (file.available()) {
    DynamicJsonDocument doc(8192);
    DeserializationError error = deserializeJson(doc, file.readString());
    if (error) {
      //Serial.print("deserializeJson() failed: ");
      //Serial.println(error.c_str());
      if (message) {
        *message = F("load_image_from_json: deserializeJson() failed.");
      }
      return false;
    }

    JsonObject object = doc.as<JsonObject>();

    for (uint16_t i = 0; i < NUM_LEDS; i++) leds[i] = 0;
    retval = deserializeSegment(object, leds, NUM_LEDS);
  }
  file.close();

  if (message) {
    *message = F("load_image_from_file(): Matrix loaded.");
  }
  return retval;
}

//******************
// PATTERN FUNCTIONS
//******************

/*
void Layer::pac_man_cb(uint8_t event) {
  static bool one_shot = false;
  switch (event) {
    case 0:
      one_shot = false;
      gnextup.type = "am";
      gnextup.filename = "/files/am/blinky.json";
      break;
    case 1:
      if (!one_shot) {
        one_shot = true;
        gnextup.type = "am";
        gnextup.filename = "/files/am/blue_ghost.json";
      }
      break;
    case 2:
      gimage_layer_alpha = 0;
      break;
    default:
      break;
  }
}
*/

void Layer::noop_cb(uint8_t event) {
  __asm__ __volatile__ ("nop");
}


// pattern layer effects
void Layer::set_plfx(uint8_t plfx) {
  set_type(Pattern_t);
  
  //gpattern_layer_enable = true;
  //gdemo_enabled = false;
  switch(plfx) {
    default:
      // fall through to next case
//      GlowSerum->set_cb(&noop_cb);
    case 0:
      // NONE does nothing. not even clear the LEDs, so set them to black once.
      // this is preferable because we don't want the NONE pattern setting the LEDs to black every time it is ran.
      //fill_solid((CRGB*)&leds[0], NUM_LEDS, CRGB::Black);
      clear();
      GlowSerum->set_pattern(NONE);
//      gpattern_layer_enable = false;
      break;
    case 1:
      //fill_rainbow(leds, NUM_LEDS, hue, 2);
      GlowSerum->set_pattern(DYNAMIC_RAINBOW);
      break;
    case 2:
      //fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));
      GlowSerum->set_pattern(SOLID);
      //GlowSerum->set_overlay(CONFETTI, false);
      //GlowSerum->set_overlay(FROZEN_DECAY, false);
      break;
    case 3:
      GlowSerum->set_pattern(ORBIT);
      break;
    case 4:
      GlowSerum->set_pattern(RUNNING_LIGHTS);
      break;
    case 5:
      GlowSerum->set_pattern(JUGGLE);
      break;
    case 6:
      GlowSerum->set_pattern(SPARKLE);
      break;
    case 7:
      GlowSerum->set_pattern(WEAVE);
      break;
    case 8:
      GlowSerum->set_pattern(CHECKERBOARD);
      break;
    case 9:
      GlowSerum->set_pattern(BINARY_SYSTEM);
      //GlowSerum->set_overlay(FROZEN_DECAY, false);
      break;
    case 10:
      GlowSerum->set_pattern(SHOOTING_STAR);
      break;
    case 11:
      GlowSerum->set_pattern(PAC_MAN);
      //GlowSerum->set_cb(&pac_man_cb);
      break;
    case 12:
      GlowSerum->set_pattern(CYLON);
      break;
    //case 99:
    //  gdemo_enabled = true;
    //  break;
  }
}


//*****************
// ACCENT FUNCTIONS
//*****************

//accent layer effects
void Layer::set_alfx(uint8_t alfx) {
  set_type(Accent_t);

  switch(alfx) {
    default:
      // fall through to next case
    case 0:
      GlowSerum->set_overlay(NO_OVERLAY, false);
      break;
    case 1:
      GlowSerum->set_overlay(GLITTER, false);
      break;
    case 2:
      GlowSerum->set_overlay(CONFETTI, false);
      break;
    case 3:
      GlowSerum->set_overlay(FLICKER, false);
      break;
    case 4:
      GlowSerum->set_overlay(FROZEN_DECAY, false);
      break;
  }
}


/*
void demo() {
  const uint8_t PATTERNS_NUM = 21;

  const Pattern patterns[PATTERNS_NUM] = {DYNAMIC_RAINBOW, SOLID, ORBIT, RUNNING_LIGHTS,
                                          JUGGLE, SPARKLE, WEAVE, CHECKERBOARD, BINARY_SYSTEM,
                                          SOLID, SOLID, SOLID, SOLID, DYNAMIC_RAINBOW,
                                          SHOOTING_STAR, //MITOSIS, BUBBLES, MATRIX,
                                          BALLS, CYLON,
                                          //STARSHIP_RACE
                                          //PAC_MAN, // PAC_MAN crashes ???
                                          //SOUND_BLOCKS, SOUND_BLOCKS, SOUND_RIBBONS, SOUND_RIBBONS,
                                          //SOUND_ORBIT, SOUND_ORBIT, SOUND_RIPPLE, SOUND_RIPPLE
                                          };



  const Overlay overlays[PATTERNS_NUM] = {NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY,
                                          NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY,
                                          GLITTER, BREATHING, CONFETTI, FLICKER, FROZEN_DECAY,
                                          NO_OVERLAY, //NO_OVERLAY, NO_OVERLAY, NO_OVERLAY,
                                          NO_OVERLAY, NO_OVERLAY
                                          //NO_OVERLAY
                                          //NO_OVERLAY
                                          //NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY,
                                          //NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY
                                          };

  static uint8_t poi = 0;

  EVERY_N_MILLISECONDS(19200) {
    poi = (poi+1) % PATTERNS_NUM;
    GlowSerum.set_pattern(patterns[poi]);
    GlowSerum.set_overlay(overlays[poi], false);

    //if (poi >= 22 && poi <= 25) {
    //    GlowSerum.set_flipflop_enabled(true);
    //}
    //else {
    //    GlowSerum.set_flipflop_enabled(false);
    //}
  }
}
*/



//**********************
// POSITIONING FUNCTIONS
//**********************

Layer::Point Layer::serp2cart(uint8_t i) {
  const uint8_t rl = 16;
  Layer::Point p;
  p.y = i/rl;
  p.x = (p.y % 2) ? (rl-1) - (i % rl) : i % rl;
  return p;
}


int16_t Layer::cart2serp(Layer::Point p) {
  const uint8_t rl = 16;
  int16_t i = (p.y % 2) ? (rl*p.y + rl-1) - p.x : rl*p.y + p.x;
  return i;
}


void Layer::flip(CRGB sm[NUM_LEDS], bool dim) {
  Layer::Point p1;
  Layer::Point p2;
  const uint8_t rl = 16;
  for (uint8_t j = 0; j < rl; j++) {
    for (uint8_t k = 0; k < rl/2; k++) {
      p1.x = dim ? j : k;
      p1.y = dim ? k : j;
      p2.x = dim ? j : rl-1-k;
      p2.y = dim ? rl-1-k : j;
      CRGB tmp = sm[cart2serp(p1)];
      sm[cart2serp(p1)] = sm[cart2serp(p2)];
      sm[cart2serp(p2)] = tmp;
    }
  }
}


//sx: + is WEST, - is EAST
//sy: + is SOUTH, - is NORTH
// where WEST means right to left, EAST means left to right, SOUTH means down, and NORTH means up
uint16_t Layer::translate(uint16_t i, int8_t xi, int8_t yi, int8_t sx, int8_t sy, bool wrap, int8_t gap) {
  // the origin of the matrix is in the NORTHEAST corner, so positive moves head WEST and SOUTH
  // this function's logic treats entering the matrix from the EAST border (WESTWARD movement) or NORTH border (SOUTHWARD movement)
  // as the basic case and flips those to get EAST and NORTH.
  // this approach simplifies the code because d and v will be positive (after transient period) 
  // which lets us just use mod to wrap the output instead of having to account for wrapping around
  // when d is positive or negative.

  if (initial) {
    initial = false;
    dx = (xi >= MD) ? -abs(xi) : xi;
    dy = (yi >= MD) ? -abs(yi) : yi;
  }

  uint16_t ti = NUM_LEDS; // used to indicate pixel is not in bounds and should not be drawn.

  Point p1 = serp2cart(i);
  Point p2;

  if (has_entered && !wrap && !visible) {
    // wrapping is not turned on and input has passed through matrix never to return
    return ti;
  }

  visible = false;
  gap = (gap == -1) ? MD : gap; // if gap is -1 set gap to MD so that the input only appears in one place but will still loop around

  uint8_t ux = (sx > 0) ? MD-1-p1.x: p1.x; // flip direction output travels
  uint8_t uy = (sy > 0) ? MD-1-p1.y: p1.y;

  int8_t vx = ux+dx; // shift input over into output by d
  int8_t vy = uy+dy;
  if (has_entered && wrap) {
    vx = vx % (MD+gap);
    vy = vy % (MD+gap);
  }

  if ( 0 <= vx && vx < MD && 0 <= vy && vy < MD ) {
    visible = true;
    vx = (sx > 0) ? MD-1-vx : vx; // flip image
    vy = (sy > 0) ? MD-1-vy : vy;
    p2.x = vx;
    p2.y = vy;
    ti = cart2serp(p2);
  }

  if (i == NUM_LEDS-1) {
    dx += abs(sx%MD);
    dy += abs(sy%MD);
    // need to track when input has entered into view for the first time
    // to prevent wrapping until the transient period has ended
    // this allows controlling how long it takes the input to enter the matrix
    // by setting abs(xi) or abs(yi) to higher numbers.
    if (dx >= 0 && dy >= 0) {
      has_entered = true;
    }

    if (has_entered && wrap) {
        dx = dx % (MD+gap);
        dy = dy % (MD+gap);
    }
  }
  return ti;
}


void Layer::ntranslate(CRGBA in[NUM_LEDS], CRGBA out[NUM_LEDS], int8_t xi, int8_t yi, int8_t sx, int8_t sy, bool wrap, int8_t gap) {
// an output array is required to use this function. out[] should be what is displayed on the matrix.
  if (initial) {
    initial = false;
    dx = (xi >= MD) ? -abs(xi) : xi;
    dy = (yi >= MD) ? -abs(yi) : yi;
  }

  Layer::Point p1;
  Layer::Point p2;

  if (has_entered && !wrap && !visible) {
    // wrapping is not turned on and input has passed through matrix never to return
    return;
  }

  visible = false;
  gap = (gap == -1) ? MD : gap; // if gap is -1 set gap to MD so that the input only appears in one place but will still loop around

  for (uint8_t j = 0; j < MD; j++) {
    for (uint8_t k = 0; k < MD; k++) {
      uint8_t ux = (sx > 0) ? MD-1-k: k; // flip direction output travels
      uint8_t uy = (sy > 0) ? MD-1-j: j;

      p1.x = ux;
      p1.y = uy;
      out[cart2serp(p1)] = CRGB::Black; // ??? TRANSPARENT 0x424242

      int8_t vx = k+dx; // shift input over into output by d
      int8_t vy = j+dy;
      if (has_entered && wrap) {
        vx = vx % (MD+gap);
        vy = vy % (MD+gap);
      }

      if ( 0 <= vx && vx < MD && 0 <= vy && vy < MD ) {
        visible = true;
        vx = (sx > 0) ? MD-1-vx : vx; // flip image
        vy = (sy > 0) ? MD-1-vy : vy;
        p2.x = vx;
        p2.y = vy;
        out[cart2serp(p1)] = in[cart2serp(p2)];
      }
      //else {
      //  out[cart2serp(p1)] = CRGB::Black; // ??? TRANSPARENT 0x424242
      //}
    }
  }

  dx += abs(sx%MD);
  dy += abs(sy%MD);
  // need to track when input has entered into view for the first time
  // to prevent wrapping until the transient period has ended
  // this allows controlling how long it takes the input to enter the matrix
  // by setting abs(xi) or abs(yi) to higher numbers.
  if (dx >= 0 && dy >= 0) {
    has_entered = true;
  }

  if (has_entered && wrap) {
      dx = dx % (MD+gap);
      dy = dy % (MD+gap);
  }
}

uint16_t Layer::director(uint16_t i) {
  uint16_t ti;
  switch (direction) {
    default:
    case 0:
      ti = i;
      break;
    case 1:
      ti = translate(i, 0, MD, 0, -1, true, -1);
      break;
    case 2:
      ti = translate(i, MD, MD, -1, -1, true, -1);
      break;
    case 3:
      ti = translate(i, MD, 0, -1, 0, true, -1);
      break;
    case 4:
      ti = translate(i, MD, -MD, -1, 1, true, -1);
      break;
    case 5:
      ti = translate(i, 0, -MD, 0, 1, true, -1);
      break;
    case 6:
      ti = translate(i, -MD, -MD, 1, 1, true, -1);
      break;
    case 7:
      ti = translate(i, -MD, 0, 1, 0, true, -1);
      break;
    case 8:
      ti = translate(i, -MD, MD, 1, -1, true, -1);
      break;
  }
  return ti;
}


//******************
// TEXT FUNCTIONS
//******************

uint8_t Layer::get_text_height(String s) {
  uint8_t min_row = MD;
  uint8_t max_row = 0;
  for (uint8_t i = 0; i < s.length(); i++) {
    char c = s[i];
    const uint8_t* glyph = font->get_bitmap(font, c);
    if (glyph) {
      uint8_t width = font->get_width(font, c);
      for (uint8_t j = 0; j < MD; j++) {
        for (uint8_t k = 0; k < width; k++) {
          uint16_t n = j*width + k;
          if(glyph[n]) {
            min_row = (j < min_row) ? j : min_row;
            max_row = (j > max_row) ? j : max_row;
            break;
          }
        }
      }
    }
  }

  if (max_row-min_row+1 > 0) {
    return max_row-min_row+1;
  }
  return 0;
}


/*
// do not delete these yet. they might be better for use with position functions.
void Layer::matrix_char(char c) {
  const uint8_t* glyph = font->get_bitmap(font, c);
  if (glyph) {
    for (uint16_t q = 0; q < NUM_LEDS; q++) {
      leds[q] = CHSV(0, 0, 0);
      //Serial.println("hello");
    }
    uint8_t width = font->get_width(font, c);
    uint16_t n = 0;
    uint8_t pad = (MD-width)/2;
    for (uint8_t i = 0; i < MD; i++) {
      for (uint8_t j = 0; j < width; j++) {
        uint8_t k = (MD-1)-pad-j;
        Layer::Point p;
        p.x = k;
        p.y = i;
        leds[cart2serp(p)] = CHSV(hue, 255, glyph[n]);
        n++;
      }
    }
  }
}

void Layer::matrix_text(String s) {
  static uint32_t pm = 0;
  static uint16_t i = 0;
  if ((millis()-pm) > 1000) {
    pm = millis();
    matrix_char(s[i]);
    i = (i+1) % s.length();
  }
}
*/


bool Layer::matrix_char_shift(char c, uint8_t vmargin) {
  if (mcs_tracking) {
    for (uint8_t i = 0; i < MD; i++) {
      for (uint8_t j = 0; j < MD-1; j++) {
        uint8_t k = (MD-1)-j;
        Layer::Point p1;
        Layer::Point p2;
        p1.x = k;
        p1.y = i;
        p2.x = k-1;
        p2.y = i;
        leds[cart2serp(p1)] = leds[cart2serp(p2)];
      }
      Layer::Point p;
      p.x = 0;
      p.y = i;
      leds[cart2serp(p)] = 0x00000000; // transparent black

    }
    mcs_tracking--;
    return false;
  }

  bool finished_shifting = false;
  uint8_t width = 0;

  const uint8_t* glyph = font->get_bitmap(font, c);
  if (glyph) {
    width = font->get_width(font, c);
    for (uint8_t i = 0; i < MD; i++) {
      for (uint8_t j = 0; j < MD-1; j++) {
        uint8_t k = (MD-1)-j;
        Layer::Point p1;
        Layer::Point p2;
        p1.x = k;
        p1.y = i;
        p2.x = k-1;
        p2.y = i;
        leds[cart2serp(p1)] = leds[cart2serp(p2)];
      }
      Layer::Point p;
      p.x = 0;
      p.y = i;
      if (vmargin <= i && i < font->h_px) {
        uint16_t n = (i-vmargin)*width + mcs_column;
        CRGB pixel = *rgb;
        pixel.scale8(glyph[n]); // not all glyph subpixels are completely off or on, so dim for those in between.
        leds[cart2serp(p)] = pixel;
        // if subpixel of glyph is whitespace make transparent, otherwise fully opaque.
        // this allows for black text.
        leds[cart2serp(p)].a = (glyph[n] == 0) ? 0 : 255;
      }
      else {
        leds[cart2serp(p)] = 0x00000000; // transparent black
      }
    }
  }

  mcs_column = (mcs_column+1)%width;
  if (mcs_column == 0) {
    finished_shifting = true;  // character fully shifted onto matrix.
    mcs_tracking = 1; // add tracking (spacing between letters) on next call
  }
  return finished_shifting;
}


//void Layer::matrix_text_shift(struct fstring ftext) {
void Layer::matrix_text_shift() {
  if ((millis()-mts_pm) > 200) {
    mts_pm = millis();
    if(matrix_char_shift(ftext.s[mts_i], ftext.vmargin)) {
      mts_i = (mts_i+1) % ftext.s.length();
    }
  }
}


void Layer::set_text(String s) {
  set_type(Text_t);
  ftext.s = s;
  ftext.vmargin = (MD-get_text_height(ftext.s))/2;
}

