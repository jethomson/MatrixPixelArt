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


void Layer::run() {
  if (rgb == nullptr) {
    Serial.println("Error: layer variable rgb was not set.");
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

  if (_ltype == Text_t) {
    matrix_text_shift();
  }

}

void Layer::clear() {
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    //leds[i] = CRGB::Black;
    leds[i] = 0x00000000;
  }
}

CRGBA Layer::get_pixel(uint16_t i) {

  CRGBA pixel_out = 0xFF000000;
  if (_ltype == Image_t || _ltype == Text_t) {
    pixel_out = leds[i];
  }
  else if (_ltype == Pattern_t || _ltype == Accent_t) {
    //CRGB pixel = ((CRGB*)&leds[0])[i];  // this works

    //CRGB* pl = (CRGB *) &leds[0];   // this works too
    //CRGB pixel = pl[i];

    // leds[] was cast as CRGB when passed to ReAnimator, so the pixels need to be
    // read back out as CRGB
    //pixel_out = pixel;
    pixel_out = ((CRGB*)leds)[i]; // does this cause crashing?
    if (pixel_out == 0xFF000000) {
      pixel_out.a = 0;
    }
  }

  return pixel_out;
}

void Layer::set_type(LayerType ltype) {
  // type 2 is an image, type 3 is text
  //if (type == 2) {
  //
  //}

  if (ltype == Pattern_t || ltype == Accent_t) {
    if (GlowSerum != nullptr) {
      delete GlowSerum;
      GlowSerum = nullptr;
    }
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

bool Layer::load_image_from_json(String json, String* message) {
  set_type(Image_t);
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
  //const char* alfx = object["alfx"]; // const char* doesn't work with Serial.print() perhaps?
  //String alfx = object["alfx"];

  //set_alfx(object["alfx"]);
  //set_plfx(object["plfx"]);

  //gimage_layer_alpha = object["ila"];

  //for (uint16_t i = 0; i < NUM_LEDS; i++) il[i] = 0;
  deserializeSegment(object, leds, NUM_LEDS);

  String plfx = object["plfx"];
  //Serial.print("plfx: ");
  //Serial.println(plfx);
  //Serial.println("load_image_from_json finished");
  //if (message) {
  // *message = F("load_image_from_json: finished.");
  //}

  //??? actual success ??? need to check deserializeSegment()
  return true;
}

bool Layer::load_image_from_file(String fs_path, String* message) {
  File file = LittleFS.open(fs_path, "r");
  
  if(!file){
    if (message) {
      *message = F("load_image_from_file(): File not found.");
    }
    return false;
  }

  if (file.available()) {
    load_image_from_json(file.readString());
  }
  file.close();

  if (message) {
    *message = F("load_image_from_file(): Matrix loaded.");
  }
  return true;
}

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


// only works for moving one pixel at a time
void Layer::move(CRGB sm[NUM_LEDS], bool dim, uint8_t d) {
  const uint8_t rl = 16;
  if ((d % rl) == 0) {
    return;
  }

  Layer::Point p1;
  Layer::Point p2;
  CRGB tmp;
  for (uint8_t j = 0; j < rl; j++) {
    for (uint8_t k = rl-1; k > 0; k--) {
      p1.x = dim ? j : k;
      p1.y = dim ? k : j;
      p2.x = dim ? j : k+d;
      p2.y = dim ? k+d : j;
      p2.x = p2.x % rl;
      p2.y = p2.y % rl;
      if (k == rl-1) {
        tmp = sm[cart2serp(p2)];
      }

      sm[cart2serp(p2)] = sm[cart2serp(p1)];

      //gone never to return
      //if (k+d < rl) {
      //  sm[cart2serp(p2)] = sm[cart2serp(p1)];
      //}
      //else {
      //  sm[cart2serp(p1)] = TRANSPARENT;
      //}
    }
    sm[cart2serp(p1)] = tmp;
  }
}



void Layer::shift(CRGB in[NUM_LEDS], CRGB out[NUM_LEDS], Direction t, bool loop, uint8_t gap) {
  // this logic treats entering the matrix from the left (LTR) or bottom (UP) as the basic case
  // and flips those to get RTL and DOWN.
  // this approach simplifies the code because d and v will be positive (after initial conditions) 
  // which lets us just use mod to loop the output instead of having to account for looping when d is
  // positive or negative.
  const uint8_t rl = 16;
  static int8_t d = -rl; // use negative for initial condition to start from outside the matrix

  if (!loop && d > rl) {
      return;
  }

  Layer::Point p1;
  Layer::Point p2;

  //bool dim = 0;
  //bool dir = 1;
  uint8_t dim = 0;
  uint8_t dir = 0;
  switch (t) {
    case RTL:
      dim = 0;
      dir = 1;
      break;
    case LTR:
      dim = 0;
      dir = 0;
      break;
    case DOWN:
      dim = 1;
      dir = 1;
      break;
    case UP:
      dim = 1;
      dir = 0;
      break;
    default:
      dim = 0;
      dir = 1;
  }

  //dir = 1; //for testing

  //static uint8_t tt = 0;
  //if (tt % 2) {
  //    dim = 0;
  //    dir = 1;
  //}
  //else  {
  //    dim = 1;
  //    dir = 0;
  //}
  //tt++;

  for (uint8_t j = 0; j < rl; j++) {
    for (uint8_t k = 0; k < rl; k++) {
      uint8_t u = dir ? rl-1-k: k; // flip direction output travels
      p1.x = dim ? j : u; // change axis output travels on
      p1.y = dim ? u : j;

      int8_t v = k+d; // shift input over into output by d
      if (loop) {
        v = v % (rl+gap);
      }

      if ( 0 <= v && v < rl ) {
        v = dir ? rl-1-v : v; // flip image
        p2.x = dim ? j : v;
        p2.y = dim ? v : j;
        //p2.x = v;
        //p2.y = v;
        out[cart2serp(p1)] = in[cart2serp(p2)];
      }
      else {
        out[cart2serp(p1)] = TRANSPARENT;
      }
    }
  }

  d++;
  if (loop) {
      d = d % (rl+gap);
  }
}



void Layer::position_OLD(CRGB in[NUM_LEDS], CRGB out[NUM_LEDS], int8_t x0, int8_t y0, bool wrap, uint8_t gap) {
  const uint8_t rl = 16;

  //if (!wrap && (x0 <= -MD || y0 <= -MD || x0 > MD || y0 > MD)) {
  //  return;
  //}

  Layer::Point p1;
  Layer::Point p2;

  int8_t u = 0;
  int8_t v = 0;

  for (uint8_t j = 0; j < MD; j++) {
    for (uint8_t k = 0; k < MD; k++) {
      p1.x = j;
      p1.y = k;
      out[cart2serp(p1)] = TRANSPARENT;

      u = j-x0;
      v = k-y0;
      if (wrap) {
        u = u % (MD+gap);
        v = v % (MD+gap);

        u = (u < -gap) ? u+MD+gap : u;
        v = (v < -gap) ? v+MD+gap : v;
      }

      if ( 0 <= u && u < MD && 0 <= v && v < MD) {
        p2.x = u;
        p2.y = v;
        out[cart2serp(p1)] = in[cart2serp(p2)];
      }
      //else {
      //  out[cart2serp(p1)] = TRANSPARENT;
      //}
    }
  }
}


// !!! TODO: need a way to reset static variables !!! perhaps some of the code can be shifted to posmove
//sx: + is RTL, - is LTR
//sy: + is DOWN, - is UP
void Layer::position(CRGB in[NUM_LEDS], CRGB out[NUM_LEDS], int8_t xi, int8_t yi, int8_t sx, int8_t sy, bool wrap, int8_t gap) {
  // the origin of the matrix is in the upper right corner, so positive moves head leftwards and downwards
  // this functions logic treats entering the matrix from the right (RTL) or top (DOWN) as the basic case
  // and flips those to get LTR and UP.
  // this approach simplifies the code because d and v will be positive (after transient period) 
  // which lets us just use mod to wrap the output instead of having to account for wrapping around
  // when d is positive or negative.
  static bool has_entered = false;
  static int8_t dx = (xi >= MD) ? -abs(xi) : xi;
  static int8_t dy = (yi >= MD) ? -abs(yi) : yi;
  static bool visible = false;
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
      out[cart2serp(p1)] = TRANSPARENT;

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
      //  out[cart2serp(p1)] = TRANSPARENT;
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


void Layer::posmove(CRGB in[NUM_LEDS], CRGB out[NUM_LEDS], int8_t xi, int8_t yi, int8_t sx, int8_t sy, bool wrap, uint8_t gap) {
    static int8_t x0 = xi;
    static int8_t y0 = yi;
    static bool _wrap = false;

    if ( abs(xi-x0) > (MD+gap) && abs(yi-y0) > (MD+gap) ) {
      Serial.println("set _wrap");
      _wrap = wrap;
    }
    //Serial.println(_wrap);
    //position_OLD(il, tl, x0, y0, _wrap, gap);
    x0 += sx;
    y0 += sy;
    //x0 = (x0+sx)%(MD+gap);
    //y0 = (y0+sy)%(MD+gap);
}


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
  static uint8_t column = 0;
  static uint8_t tracking = 0; // spacing between letters

  if (tracking) {
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
    tracking--;
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
        uint16_t n = (i-vmargin)*width + column;
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

  column = (column+1)%width;
  if (column == 0) {
    finished_shifting = true;  // character fully shifted onto matrix.
    tracking = 1; // add tracking (spacing between letters) on next call
  }
  return finished_shifting;
}


//void Layer::matrix_text_shift(struct fstring ftext) {
void Layer::matrix_text_shift() {
  //Serial.println("mts");
  static uint32_t pm = 0;
  static uint16_t i = 0;
  if ((millis()-pm) > 200) {
    pm = millis();
    if(matrix_char_shift(ftext.s[i], ftext.vmargin)) {
      i = (i+1) % ftext.s.length();
    }
  }
}


void Layer::set_text(String s) {
  set_type(Text_t);
  ftext.s = s;
  ftext.vmargin = (MD-get_text_height(ftext.s))/2;
}

