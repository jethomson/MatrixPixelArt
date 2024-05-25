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

#include <Arduino.h>
#include <FastLED.h>
#include <LittleFS.h>
#include <time.h>

#include "FastLED_RGBA.h"
#include "ReAnimator.h"
#include "ArduinoJson-v6.h"
#include "JSON_Image_Decoder.h"

const char* timezone = "EST5EDT,M3.2.0,M11.1.0";


ReAnimator::ReAnimator() : freezer(*this) {
//ReAnimator::ReAnimator(CRGB *color, uint16_t led_strip_milliamps) : freezer(*this) {

    //rgb = color;
    //CHSV chsv = rgb2hsv_approximate(*color);
    //hue = chsv.h;
    //selected_led_strip_milliamps = led_strip_milliamps;

    homogenized_brightness = 255;

    pattern = NONE;
    transient_overlay = NO_OVERLAY;
    persistent_overlay = NO_OVERLAY;

#if !defined(LEFT_TO_RIGHT_IS_FORWARD) || LEFT_TO_RIGHT_IS_FORWARD
    direction_fp = &ReAnimator::forwards;
    antidirection_fp = &ReAnimator::backwards;
#else
    direction_fp = &ReAnimator::backwards;
    antidirection_fp = &ReAnimator::forwards;
#endif

    reverse = false;

    //error: invalid conversion from 'int' to 'Pattern' [-fpermissive]
    // setting to NULL throws an error.
    // I believe NULL was resolved to 0 anyway before I switch to over to pio.
    // I believe I used NULL to try to be explicit that Patterns and Overlays weren't set yet.
    //last_pattern_ran = NULL;
    //last_pattern_ran = 0;

    autocycle_enabled = false;
    autocycle_previous_millis = 0;
    autocycle_interval = 30000;

    flipflop_enabled = false;
    flipflop_previous_millis = 0;
    flipflop_interval = 6000;

    //previous_sample = 0;
    //sample_peak = 0;
    //sample_average = 0;
    //sample_threshold = 20;
    //sound_value_gain = SOUND_VALUE_GAIN_INITIAL;

    clear();
}


ReAnimator::Freezer::Freezer(ReAnimator &r) : parent(r) {
    m_frozen = false;
    m_frozen_previous_millis = 0;
}


void ReAnimator::setup(LayerType ltype, int8_t id) {
  // we want patterns (and accents) to persist from one composite to another if they are on the same layer.
  // this allows for a pattern to play continuously (i.e without restarting) when the next item in a playlist is loaded.
  //
  // scrolling text uses an id of -1 because we always want to clear() when new text is set.
  // images use an id of -2 because real image ids (paths) are more complicated and it is unnecessary to clear since a new image should completely overwrite leds[]
  // in practice there is probably no observable difference between -2 and -1 for images.
  // could possibly get an interesting effect with -2 if the new image does not write to all of leds[] and part of the previous image remains.
  if (id == -1 || (_ltype != ltype && _id != id)) {
    _id = id;
    _ltype = ltype;

    // since layers are reused the remnants of the old effect may still be in leds[]
    // these leftovers may not be overwritten by the new effect, so it is best to clear leds[]
    clear();

    set_autocycle_interval(10000);
    set_autocycle_enabled(false);
    set_flipflop_interval(6000);
    set_flipflop_enabled(false);

    set_pattern(NONE);
    set_overlay(NO_OVERLAY, false);
    set_cb(noop_cb);
  }
}


void ReAnimator::set_selected_led_strip_milliamps(uint16_t led_strip_milliamps) {
    if (led_strip_milliamps > selected_led_strip_milliamps) {
        // normally homogenized_brightness only goes down but since the power is increased we need to reset homogenized_brightness so it can
        // learn the new brightness level that makes all the animations have a consistent brightness
        //homogenized_brightness = calculate_max_brightness_for_power_vmA(leds, NUM_LEDS, 255, LED_STRIP_VOLTAGE, led_strip_milliamps);
        homogenized_brightness = 128;
    }
    else {
        //homogenized_brightness = calculate_max_brightness_for_power_vmA(leds, NUM_LEDS, homogenized_brightness, LED_STRIP_VOLTAGE, led_strip_milliamps);
        homogenized_brightness = 128;
    }
    selected_led_strip_milliamps = led_strip_milliamps;
}


// When FastLED's power management functions are used FastLED dynamically adjusts the brightness level to be as high as possible while
// keeping the power draw near the specified level. This can lead to the brightness level of an animation noticeably increasing when
// fewer LEDs are lit and the brightness noticeably dipping when more LEDs are lit or their colors change.
// homogenize_brightness() learns the lowest brightness level of all the animations and uses it across every animation to keep a consistent
// brightness level. This will lead to dimmer animations and power usage almost always a good bit lower than what the FastLED power
// management function was set to aim for. Set the #define for HOMOGENIZE_BRIGHTNESS to false to disable this feature.
void ReAnimator::homogenize_brightness() {
    //uint8_t max_brightness = calculate_max_brightness_for_power_vmA(leds, NUM_LEDS, homogenized_brightness, LED_STRIP_VOLTAGE, selected_led_strip_milliamps);
    uint8_t max_brightness = 128; // for testing
    if (max_brightness < homogenized_brightness) {
        homogenized_brightness = max_brightness;
    }
}


uint32_t ReAnimator::get_autocycle_interval() {
    return autocycle_interval;
}


void ReAnimator::set_autocycle_interval(uint32_t inteval) {
    autocycle_interval = inteval;
    autocycle_previous_millis = 0; // set to zero so autocycling will start without waiting
}


bool ReAnimator::get_autocycle_enabled() {
    return autocycle_enabled;
}


void ReAnimator::set_autocycle_enabled(bool enabled) {
    autocycle_enabled = enabled;
    autocycle_previous_millis = 0; // set to zero so autocycling will start without waiting
}



uint32_t ReAnimator::get_flipflop_interval() {
    return flipflop_interval;
}


void ReAnimator::set_flipflop_interval(uint32_t inteval) {
    flipflop_interval = inteval;
    flipflop_previous_millis = 0; // set to zero so flipfloping will start without waiting
}


bool ReAnimator::get_flipflop_enabled() {
    return flipflop_enabled;
}


void ReAnimator::set_flipflop_enabled(bool enabled) {
    flipflop_enabled = enabled;
    flipflop_previous_millis = 0; // set to zero so flipfloping will start without waiting
}



Pattern ReAnimator::get_pattern() {
    return pattern;
}


int8_t ReAnimator::set_pattern(Pattern pattern_in) {
    return set_pattern(pattern_in, false, true);
}


int8_t ReAnimator::set_pattern(Pattern pattern_in, bool reverse) {
    return set_pattern(pattern_in, reverse, true);
}


int8_t ReAnimator::set_pattern(Pattern pattern_in, bool reverse_in, bool disable_autocycle_flipflop) {
    Pattern pattern_out;
    Overlay overlay_out;

    int8_t retval = 0;

    if (autocycle_enabled && pattern_in == NONE) {
        pattern_in = static_cast<Pattern>(pattern+1);
    }

    switch(pattern_in) {
        default:
            retval = INT8_MIN;
            // fall through to next case
        case ORBIT:
            pattern_out = ORBIT;
            overlay_out = NO_OVERLAY;
            break;
        case CHECKERBOARD:
            pattern_out = CHECKERBOARD;
            overlay_out = NO_OVERLAY;
            break;
        case BINARY_SYSTEM:
            pattern_out = BINARY_SYSTEM;
            overlay_out = NO_OVERLAY;
            break;
        case THEATER_CHASE:
            pattern_out = THEATER_CHASE;
            overlay_out = NO_OVERLAY;
            break;
        case RUNNING_LIGHTS:
            pattern_out = RUNNING_LIGHTS;
            overlay_out = NO_OVERLAY;
            break;
        case SHOOTING_STAR:
            pattern_out = SHOOTING_STAR;
            overlay_out = NO_OVERLAY;
            break;
        case CYLON:
            pattern_out = CYLON;
            overlay_out = NO_OVERLAY;
            break;
        case SOLID:
            pattern_out = SOLID;
            overlay_out = NO_OVERLAY;
            break;
        case JUGGLE:
            pattern_out = JUGGLE;
            overlay_out = NO_OVERLAY;
            break;
        case RIFFLE:
            pattern_out = RIFFLE;
            overlay_out = NO_OVERLAY;
            break;
        case MITOSIS:
            pattern_out = MITOSIS;
            overlay_out = NO_OVERLAY;
            break;
        case BUBBLES:
            pattern_out = BUBBLES;
            overlay_out = NO_OVERLAY;
            break;
        case SPARKLE:
            pattern_out = SPARKLE;
            overlay_out = NO_OVERLAY;
            break;
        case MATRIX:
            clear();
            pattern_out = MATRIX;
            overlay_out = NO_OVERLAY;
            break;
        case WEAVE:
            pattern_out = WEAVE;
            overlay_out = NO_OVERLAY;
            break;
        case STARSHIP_RACE:
            pattern_out = STARSHIP_RACE;
            overlay_out = NO_OVERLAY;
            break;
        case PUCK_MAN:
            pattern_out = PUCK_MAN;
            overlay_out = NO_OVERLAY;
            break;
        case BALLS:
            pattern_out = BALLS;
            overlay_out = NO_OVERLAY;
            break;
        case HALLOWEEN_FADE:
            pattern_out = HALLOWEEN_FADE;
            overlay_out = NO_OVERLAY;
            break;
        case HALLOWEEN_ORBIT:
            pattern_out = HALLOWEEN_ORBIT;
            overlay_out = NO_OVERLAY;
            break;
        case NONE:
            pattern_out = NONE;
            overlay_out = NO_OVERLAY;
            break;
        //case SOUND_RIBBONS:
        //    pattern_out = SOUND_RIBBONS;
        //    overlay_out = NO_OVERLAY;
        //    break;
        //case SOUND_RIPPLE:
        //    pattern_out = SOUND_RIPPLE;
        //    overlay_out = NO_OVERLAY;
        //    break;
        //case SOUND_ORBIT:
        //    pattern_out = SOUND_ORBIT;
        //    overlay_out = NO_OVERLAY;
        //    break;
        //case SOUND_BLOCKS:
        //    pattern_out = SOUND_BLOCKS;
        //    overlay_out = NO_OVERLAY;
        //    break;
        case DYNAMIC_RAINBOW:
            pattern_out = DYNAMIC_RAINBOW;
            overlay_out = NO_OVERLAY;
            break;
    }

    pattern = pattern_out;
    set_overlay(overlay_out, false);

    reverse = reverse_in;

    if (disable_autocycle_flipflop) {
        set_autocycle_enabled(false);
        set_flipflop_enabled(false);
    }

    return retval;
}


int8_t ReAnimator::increment_pattern() {
    return increment_pattern(true);
}


int8_t ReAnimator::increment_pattern(bool disable_autocycle_flipflop) {
    return set_pattern(static_cast<Pattern>(pattern+1), reverse, disable_autocycle_flipflop);
}



Overlay ReAnimator::get_overlay(bool is_persistent) {
    if (is_persistent) {
        return persistent_overlay;
    }
    else {
        return transient_overlay;
    }
}


int8_t ReAnimator::set_overlay(Overlay overlay_in, bool is_persistent) {
    Overlay overlay_out;
    int8_t retval = 0;

    switch(overlay_in) {
        default:
            retval = INT8_MIN;
            // fall through to next case
        case NO_OVERLAY:
            overlay_out = NO_OVERLAY;
            break;
        case GLITTER:
            overlay_out = GLITTER;
            break;
        case BREATHING:
            overlay_out = BREATHING;
            break;
        case CONFETTI:
            overlay_out = CONFETTI;
            break;
        case FLICKER:
            overlay_out = FLICKER;
            break;
        case FROZEN_DECAY:
            overlay_out = FROZEN_DECAY;
            break;
    }

    if (is_persistent) {
        persistent_overlay = overlay_out;
    }
    else {
        transient_overlay = overlay_out;
    }

    return retval;
}


void ReAnimator::increment_overlay(bool is_persistent) {
    if (is_persistent) {
        set_overlay(static_cast<Overlay>(persistent_overlay+1), is_persistent);
    }
    else {
        set_overlay(static_cast<Overlay>(transient_overlay+1), is_persistent);
    }
}


bool ReAnimator::set_image(String fs_path, String* message) {
  bool retval = false;
  File file = LittleFS.open(fs_path, "r");

  if(!file){
    if (message) {
      *message = F("set_image(): File not found.");
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
        *message = F("set_image: deserializeJson() failed.");
      }
      return false;
    }

    JsonObject object = doc.as<JsonObject>();

    for (uint16_t i = 0; i < NUM_LEDS; i++) leds[i] = 0xFFFFFF00;
    retval = deserializeSegment(object, leds, NUM_LEDS);
  }
  file.close();

  if (message) {
    *message = F("set_image(): Matrix loaded.");
  }
  return retval;
}


void ReAnimator::set_text(String s) {
  // need to reinitialize when one string was already being written and new string is set
  refresh_text_index = 0; // start at the beginning of a string
  shift_char_column = 0; // start at the beginning of a glyph

  ftext.s = s;
  ftext.vmargin = MD/2 - get_text_center(ftext.s);
}


void ReAnimator::set_info(Info type) {
    _id = type;
    switch(type) {
      default:
          // fall through to next case
      case TIME_12HR:
      case TIME_24HR:
      case DATE_MMDD:
      case DATE_DDMM:
      case TIME_12HR_DATE_MMDD:
      case TIME_24HR_DATE_DDMM:
          setup_clock();
          break;
      //case 6:
        // maybe do scrolling count down here
        //break;
      //case 7:
        // maybe do weather info
        //break;
    }
}


void ReAnimator::set_color(CRGB *color) {
    rgb = color;
    CHSV chsv = rgb2hsv_approximate(*color);
    hue = chsv.h;
}


void ReAnimator::set_color(CRGB color) {
  internal_rgb = color;
  rgb = &internal_rgb;
  set_color(rgb);
}


void ReAnimator::set_cb(void(*cb)(uint8_t)) {
    _cb = cb;
}


void ReAnimator::set_heading(uint8_t h) {
  // initial only needs to be set to true, which resets the translation variables, if the heading changes.
  // this allows for one image to be swapped in for another while maintaining the same position and path.
  t_initial = (heading != h);
  heading = h;
}


//void ReAnimator::set_sound_value_gain(uint8_t gain) {
//    sound_value_gain = gain;
//}



void ReAnimator::clear() {
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGBA::Transparent;
  }
}


void ReAnimator::reanimate() {
    //if (rgb == nullptr) {
    //  internal_rgb = CRGB::Yellow;
    //  rgb = &internal_rgb;
    //}
    CHSV chsv = rgb2hsv_approximate(*rgb);
    hue = chsv.h; // !!BUG!! if color is 0x000000 (black) then hue will be 0 which is red when CHSV(hue, 255, 255)

    if (autocycle_enabled) {
        autocycle();
    }

    if (flipflop_enabled) {
        flipflop();
    }

    //process_sound();

    if (!freezer.is_frozen()) {
        if (_ltype == Pattern_t) {
          run_pattern(pattern);
          last_pattern_ran = pattern;
        }
        
        if (_ltype == Text_t) {
            refresh_text(200);
        }

        if (_ltype == Info_t) {
            refresh_info(200);
        }
    }

    apply_overlay(transient_overlay);
    apply_overlay(persistent_overlay);

    //print_dt();

#if HOMOGENIZE_BRIGHTNESS
    homogenize_brightness();
#endif

    if (transient_overlay != BREATHING && transient_overlay != FLICKER && persistent_overlay != BREATHING && persistent_overlay != FLICKER) {
        FastLED.setBrightness(homogenized_brightness);
    }
}


CRGBA ReAnimator::get_pixel(uint16_t i) {
  //CRGBA pixel_out = 0xFF000000; // if black with no transparency is used it creates a sort of spotlight effect
  CRGBA pixel_out = 0x00000000;
  uint16_t ti = mover(i);
  if (0 <= ti && ti < NUM_LEDS) {
    pixel_out = leds[ti];
  }

  //if (_ltype == Pattern_t || _ltype == Accent_t) {
    //pixel_out.a = 255; // work around to make sure every pixel is opaque. need to work on ReAnimator so it uses transparency better.
    //if (pixel_out == 0xFF000000) {
    //  pixel_out.a = 0;
    //}
  //}

  return pixel_out;
}


//***********
//* PRIVATE *
//***********
int8_t ReAnimator::run_pattern(Pattern pattern) {
    int8_t retval = 0;
    uint8_t orbit_delta = !reverse ? 1 : -1;
    uint16_t(ReAnimator::*dfp)(uint16_t) = !reverse ? direction_fp : antidirection_fp;

    switch(pattern) {
        default:
            retval = INT8_MIN;
            // fall through to next case
        case ORBIT:
            orbit(20, orbit_delta);
            break;
        case CHECKERBOARD:
            //general_chase(350, 2, dfp);
            accelerate_decelerate_pattern(200, 10, 1000, 2, &ReAnimator::general_chase, dfp);
            break;
        case BINARY_SYSTEM:
            //general_chase(350, 16, dfp);
            accelerate_decelerate_pattern(200, 10, 1000, 16, &ReAnimator::general_chase, dfp);
            break;
        case THEATER_CHASE:
            //theater_chase(350, dfp);
            //general_chase(350, 3, dfp);
            accelerate_decelerate_pattern(200, 10, 1000, 3, &ReAnimator::general_chase, dfp);
            break;
        case RUNNING_LIGHTS:
            //running_lights(30, dfp);
            accelerate_decelerate_pattern(30, 2, 1000, 3, &ReAnimator::running_lights, dfp);
            break;
        case SHOOTING_STAR:
            //if one star traverses all the LEDs (worst case) and the star moves one LED ever draw_interval
            //then it takes NUM_LEDS*draw_interval for one star sequence to be completely drawn in milliseconds
            //draw_time = NUM_LEDS*draw_interval [ms]
            //if we want to draw N stars per minute [spm] then N * draw_time must be less than 60000 [ms]
            // if NUM_LEDS is higher that means the draw_time is longer and so the spm must be lower to fit all
            // those stars in the 60000 [ms] window

            //50[spm]*52[LEDs]*5[ms] = 13000[ms]
            //(60000-13000)/50 = 940 [ms] time between stars
            //shooting_star(5, 5, 40, 50, dfp); // original


            //27[spm]*256[LEDs]*5[ms] = 34560[ms]
            //(60000-34560)/27 = 942 [ms] time between stars
            shooting_star(5, 5, 40, 27, dfp); // to more leds increase the delay between stars too much?
            break;
        case CYLON:
            cylon(20, dfp);
            break;
        case SOLID:
            solid(200);
            break;
        case JUGGLE:
            juggle();
            break;
        case RIFFLE:
            riffle();
            break;
        case MITOSIS:
            mitosis(50, 1);
            break;
        case BUBBLES:
            bubbles(100, dfp);
            break;
        case SPARKLE:
            sparkle(20, false, 32);
            break;
        case MATRIX:
            matrix(50);
            break;
        case WEAVE:
            weave(60);
            break;
        case STARSHIP_RACE:
            starship_race(88, dfp);
            break;
        case PUCK_MAN:
            puck_man(150, dfp);
            break;
        case BALLS:
            bouncing_balls(40, dfp);
            break;
        case HALLOWEEN_FADE:
            halloween_colors_fade(50);
            break;
        case HALLOWEEN_ORBIT:
            halloween_colors_orbit(20, orbit_delta);
            break;
        case NONE:
            //fill_solid(leds, NUM_LEDS, CRGBA::Transparent);
            break;
        //case SOUND_RIBBONS:
        //    sound_ribbons(30);
        //    break;
        //case SOUND_RIPPLE:
        //    sound_ripple(100, (sample_peak==1));
        //    break;
        //case SOUND_BLOCKS:
        //    sound_blocks(50, (sound_value > 32));
        //    break;
        //case SOUND_ORBIT:
        //    sound_orbit(30, dfp);
        //    break;
        case DYNAMIC_RAINBOW:
            //accelerate_decelerate_pattern(30, 2, 1000, &ReAnimator::dynamic_rainbow, dfp);
            dynamic_rainbow(50, dfp);
            break;
    }

    return retval;
}


int8_t ReAnimator::apply_overlay(Overlay overlay) {
    int8_t retval = 0;

    switch(overlay) {
        default:
            retval = INT8_MIN;
            // fall through to next case
        case NO_OVERLAY:
            break;
        case GLITTER:
            glitter(700);
            break;
        case BREATHING:
            breathing(10);
            break;
        case CONFETTI:
            sparkle(20, true, 0);
            break;
        case FLICKER:
            flicker(150);
            break;
        case FROZEN_DECAY:
            freezer.timer(7000);
            if (freezer.is_frozen()) {
                fade_randomly(7, 100);
            }
            break;
    }

    return retval;
}


//void ReAnimator::shift_text(struct fstring ftext) {
void ReAnimator::refresh_text(uint16_t draw_interval) {
    if (is_wait_over(draw_interval)) {
        if(shift_char(ftext.s[refresh_text_index], ftext.vmargin)) {
            refresh_text_index = (refresh_text_index+1) % ftext.s.length();
        }
    }
}


void ReAnimator::refresh_info(uint16_t draw_interval) {
    switch(_id) {
      default:
          // fall through to next case
      case TIME_12HR:
      case TIME_24HR:
      case DATE_MMDD:
      case DATE_DDMM:
      case TIME_12HR_DATE_MMDD:
      case TIME_24HR_DATE_DDMM:
          refresh_date_time(draw_interval);
          break;
      //case 6:
        // maybe do scrolling count down here
        //break;
      //case 7:
        // maybe do weather info
        //break;
    }
}



// ++++++++++++++++++++++++++++++
// ++++++++++ PATTERNS ++++++++++
// ++++++++++++++++++++++++++++++

void ReAnimator::orbit(uint16_t draw_interval, int8_t delta) {
    static uint16_t pos = NUM_LEDS;
    static uint8_t loop_num = 0;

    if (pattern != last_pattern_ran) {
        pos = NUM_LEDS;
    }

    if (is_wait_over(draw_interval)) {
        fadeToBlackBy(leds, NUM_LEDS, 20);

        if (delta > 0) {
            pos = pos % NUM_LEDS;
        }
        else {
            // pos underflows after it goes below zero
            if (pos > NUM_LEDS-1) {
                pos = NUM_LEDS-1;
            }
        }

        //leds[pos] = CHSV(*hue, 255, 255);
        leds[pos] = *rgb;
        pos = pos + delta;

        loop_num = (pos == NUM_LEDS) ? loop_num+1 : loop_num; 
    }
}

/*
void ReAnimator::theater_chase_old(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    static uint16_t delta = 0;

    if (is_wait_over(draw_interval)) {
        fadeToBlackBy(leds, NUM_LEDS, 230);

        for (uint16_t i = 0; i+delta < NUM_LEDS; i=i+3) {
            leds[(this->*dfp)(i+delta)] = CHSV(hue, 255, 255);
        }

        delta = (delta + 1) % 3;
    }
}
*/


//3 is confusing
//2 gives a checkerboard
//4 is OK
//16, 128 is cool
void ReAnimator::theater_chase(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    static uint16_t delta = 0;

    if (is_wait_over(draw_interval)) {
        fadeToBlackBy(leds, NUM_LEDS, 128);

        for (uint16_t i = 0; i+delta < NUM_LEDS; i=i+16) {
            //leds[(this->*dfp)(i+delta)] = CHSV(*hue, 255, 255);
            leds[(this->*dfp)(i+delta)] = *rgb;
        }

        delta = (delta + 1) % 16;
    }
}


void ReAnimator::general_chase(uint16_t draw_interval, uint16_t genparam, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    //genparam represents the number of LEDs involved in a pattern
    static uint16_t delta = 0;

    if (is_wait_over(draw_interval)) {
        fadeToBlackBy(leds, NUM_LEDS, (255-(genparam*8)));

        for (uint16_t i = 0; i+delta < NUM_LEDS; i=i+genparam) {
            //leds[(this->*dfp)(i+delta)] = CHSV(*hue, 255, 255);
            leds[(this->*dfp)(i+delta)] = *rgb;
        }

        delta = (delta + 1) % genparam;
    }
}


void ReAnimator::running_lights(uint16_t draw_interval, uint16_t genparam, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    //genparam represents the number of waves
    static uint16_t delta = 0;

    if (is_wait_over(draw_interval)) {
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            uint16_t a = genparam*(i+delta)*255/(NUM_LEDS-1);
            // this pattern normally runs from right-to-left, so flip it by using negative indexing
            uint16_t ni = (NUM_LEDS-1) - i;
            //leds[(this->*dfp)(ni)] = CHSV(*hue, 255, sin8(a));
            CRGBA light = *rgb;
            nscale8x3(light.r, light.g, light.b, 255-sin8(a));
            leds[(this->*dfp)(ni)] = light;
        }

        delta = (delta + 1) % (NUM_LEDS/genparam);
    }
}


//star_size – the number of LEDs that represent the star, not counting the tail of the star.
//star_trail_decay - how fast the star trail decays. A larger number makes the tail short and/or disappear faster.
//spm - stars per minute
void ReAnimator::shooting_star(uint16_t draw_interval, uint8_t star_size, uint8_t star_trail_decay, uint8_t spm, uint16_t(ReAnimator::*dfp)(uint16_t)) {  
    static uint16_t start_pos = random16(0, NUM_LEDS/4);
    static uint16_t stop_pos = random16(star_size+(NUM_LEDS/2), NUM_LEDS);

    const uint16_t cool_down_interval = (60000-(spm*NUM_LEDS*draw_interval))/spm; // adds a delay between creation of new shooting stars
    static uint32_t cdi_pm = 0; // cool_down_interval_previous_millis

    static uint16_t pos = start_pos;

    if (pattern != last_pattern_ran) {
        start_pos = random16(0, NUM_LEDS/4);
        stop_pos = random16(star_size+(NUM_LEDS/2), NUM_LEDS);
        pos = start_pos;
        //cdi_pm doesn't need to be reset here because it's a cool down timer
    }

    if (is_wait_over(draw_interval)) {
        fade_randomly(128, star_trail_decay);

        if ( (millis() - cdi_pm) > cool_down_interval ) {
            for (uint8_t i = 0; i < star_size; i++) {
                //leds[(this->*dfp)(pos+(star_size-1)-i)] += CHSV(*hue, 255, 255);
                leds[(this->*dfp)(pos+(star_size-1)-i)] += *rgb;
                // we have to subtract 1 from star_size because one piece goes at pos
                // example, if star_size = 3: [*]  [*]  [*]
                //                            pos pos+1 pos+2
            }
            pos++;
            if (pos+(star_size-1) >= stop_pos+1) {
                start_pos = random16(0, NUM_LEDS/4);
                stop_pos = random16(star_size+(NUM_LEDS/2), NUM_LEDS);
                pos = start_pos;
                cdi_pm = millis();
            }
        }
    }
}


void ReAnimator::cylon(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    static uint16_t pos = 0;
    static int8_t delta = 1;

    if (pattern != last_pattern_ran) {
        pos = 0;
        delta = 1;
    }

    if (is_wait_over(draw_interval)) {
        fadeToBlackBy(leds, NUM_LEDS, 20);

        //leds[(this->*dfp)(pos)] += CHSV(*hue, 255, 192);
        leds[(this->*dfp)(pos)] += *rgb;

        pos = pos + delta;
        if (pos == 0 || pos == NUM_LEDS-1) {
            delta = -delta;
        }
    }
}


void ReAnimator::solid(uint16_t draw_interval) {
    if (is_wait_over(draw_interval)) {
        //fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));
        fill_solid(leds, NUM_LEDS, *rgb);
        
        
        //CHSV chsv = rgb2hsv_approximate(*rgb);
        //chsv.h += 128;
        //fill_solid(leds, NUM_LEDS, chsv);
    }
}



// borrowed from FastLED/examples/DemoReel00.ino -Mark Kriegsman, December 2014
void ReAnimator::juggle() {
    // eight colored dots, weaving in and out of sync with each other
    fadeToBlackBy(leds, NUM_LEDS, 20);
    byte dothue = 0;
    for(uint8_t i = 0; i < 1; i++) {
        leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
        dothue += 32;
    }
}



void ReAnimator::riffle() {
    fadeToBlackBy(leds, NUM_LEDS, 5);
    uint8_t dothue = hue;
    uint8_t i = 0;
    while (true) {
        uint16_t high = ((i+1)*16)-1;
        if (high >= NUM_LEDS/2) {
            break;
        }
        uint16_t p = beatsin16(3, 0, high);
        leds[NUM_LEDS-1-p] |= CHSV(dothue, 200, 255);

        uint16_t q = (MD*(p/MD)+MD)-1 - p%MD;
        leds[q] |= CHSV(dothue, 200, 255);

        i++;
        dothue += 32;
    }
}


void ReAnimator::mitosis(uint16_t draw_interval, uint8_t cell_size) {
    const uint16_t start_pos = NUM_LEDS/2;
    static uint16_t pos = start_pos;

    if (pattern != last_pattern_ran) {
        pos = start_pos;
    }

    if (is_wait_over(draw_interval)) {
        fadeToBlackBy(leds, NUM_LEDS, 30);

        for (uint8_t i = 0; i < cell_size; i++) {
            uint16_t pi = pos+(cell_size-1)-i;
            uint16_t ni = (NUM_LEDS-1) - pi;
            //leds[pi] = CHSV(*hue, 255, 255);
            //leds[ni] = CHSV(*hue, 255, 255);
            leds[pi] = *rgb;
            leds[ni] = *rgb;
        }
        pos++;
        if (pos+(cell_size-1) >= NUM_LEDS) {
            pos = start_pos;
        }
    }
}


void ReAnimator::bubbles(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    const uint8_t num_bubbles = 8;
    static uint8_t bubble_time[num_bubbles] = {};

    if (pattern != last_pattern_ran) {
        // ??? bubble_time[num_bubbles] = memset(bubble_time, 0, sizeof(bubble_time));
        memset(bubble_time, 0, sizeof(bubble_time));

    }

    if (is_wait_over(draw_interval)) {
        clear();

        for (uint8_t i = 0; i < num_bubbles; i++) {
            if (bubble_time[i] == 0 && random8(33) == 0) {
                bubble_time[i] = random8(1, sqrt16(UINT16_MAX/NUM_LEDS));
            }

            if (bubble_time[i] > 0) {
                uint8_t t = bubble_time[i];

                uint16_t d = t*(t+1);

                if (t == UINT8_MAX) {
                    d = UINT16_MAX;
                }

                uint16_t pos = lerp16by16(0, NUM_LEDS-1, d);
                leds[(this->*dfp)(pos)] += CHSV(i*(256/num_bubbles) + hue, 255, 192);
                motion_blur((3*pos)/NUM_LEDS, pos, dfp);

                if (t < UINT8_MAX) {
                    t+=10;
                    if (t > bubble_time[i]) { // make sure overflow didn't happen
                        bubble_time[i] = t;
                    }
                    else {
                        bubble_time[i] = UINT8_MAX;
                    }
                }
                else {
                    bubble_time[i] = 0;
                }
            }        
        }
    }
}


void ReAnimator::sparkle(uint16_t draw_interval, bool random_color, uint8_t fade) {
    CRGB c = (random_color) ? CHSV(random8(), 255, 255) : *rgb;

    // it's necessary to use finished_waiting() here instead of is_wait_over()
    // because sparkle can be an overlay
    if (finished_waiting(draw_interval)) {
        fadeToBlackBy(leds, NUM_LEDS, fade);
        leds[random16(NUM_LEDS)] = c;
    }
}


// resembles the green code from The Matrix
void ReAnimator::matrix(uint16_t draw_interval) {
    if (is_wait_over(draw_interval)) {
        memmove(&leds[1], &leds[0], (NUM_LEDS-1)*sizeof(CRGB));

        if (random8() > 205) {
            leds[0] = CHSV(HUE_GREEN, 255, 255);
            //leds[0] = CRGBA::Green; // is this noticeably different from HUE_GREEN ?
            leds[0] = 0x00FF40; // result of using a HSV to RGB calculator to convert 96 (HUE_GREEN) to RGB. (96/256)*360 = 135. 135, 100%, 100% --> 0x00FF40
        }
        else {
            leds[0] = CRGBA::Transparent;
        }
    }
}


void ReAnimator::weave(uint16_t draw_interval) {
    static uint16_t pos = 0;

    if (pattern != last_pattern_ran) {
        pos = 0;
    }

    if (is_wait_over(draw_interval)) {
        fadeToBlackBy(leds, NUM_LEDS, 20);

        //leds[pos] += CHSV(*hue, 255, 128);
        //leds[NUM_LEDS-1-pos] += CHSV(*hue+(HUE_PURPLE-HUE_ALIEN_GREEN), 255, 128);
        leds[pos] += *rgb;
        leds[NUM_LEDS-1-pos] += (CRGBA::White - *rgb);
        pos = (pos + 2) % NUM_LEDS;
    }
}


void ReAnimator::starship_race(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    const uint16_t race_distance = (11*UINT8_MAX)/2; // 7/2 -> 3.5 laps
    const uint8_t total_starships = 5;
    // UINT8_MAX/NUM_LEDS is the speed required for a starship to move one LED per redraw
    const uint8_t range = ceil(static_cast<float>(UINT8_MAX)/NUM_LEDS);
    const uint8_t speed_boost_period = 4; // every N redraws speed_boost is increased

    static Starship starships[total_starships];
    static bool go = true;
    static uint8_t redraw_count = 0;
    static uint8_t speed_boost = 0;
    static uint8_t count_down = 0;

    if (pattern != last_pattern_ran) {
        for (uint8_t i = 0; i < total_starships; i++) {
            starships[i].distance = 0;
            starships[i].color = i*(256/total_starships);
        }
        go = true;
        redraw_count = 0;
        speed_boost = 0;
        count_down = 0;
    }

    if (is_wait_over(draw_interval)) {
        if (go) {
            clear();

            for (uint8_t i = 0; i < total_starships; i++) {
                // current_total_distance = previous_total_distance + speed*delta_time, delta_time is always 1
                starships[i].distance = starships[i].distance + random8(speed_boost, (range+speed_boost)+1);
            }

            // sort starships by distance travelled in descending order
            qsort(starships, total_starships, sizeof(Starship), compare);

            for (uint8_t i = 0; i < total_starships; i++) {
                uint16_t pos = lerp16by8(0, NUM_LEDS-1, starships[i].distance);

                // we don't want multiple starships' position to be on the same LED
                // if an LED is already lit then a starship is already there so
                // move backwards until we find an unlit LED
                while (leds[(this->*dfp)(pos)] != CRGBA::Transparent && pos > 0) {
                    pos--;
                }
                leds[(this->*dfp)(pos)] = CHSV(starships[i].color, 255, 255);
            }

            redraw_count++;
            if (redraw_count == speed_boost_period) {
                redraw_count = 0;
                speed_boost++;
            }
        }
        else {
            // next race will happen after count_down*draw_interval has elapsed
            count_down--;
            if (count_down == 0) {
                go = true;
            }
        }

        if (starships[0].distance >= race_distance) {
            // race is finished
            fill_solid(leds, NUM_LEDS, CHSV(starships[0].color, 255, 255));

            for (uint8_t i = 0; i < total_starships; i++) {
                starships[i].distance = 0;
                starships[i].color = i*(256/total_starships);
            }

            go = false;
            redraw_count = 0;
            speed_boost = 0;
            count_down = 10;
        }
    }
}


void ReAnimator::puck_man(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    static uint16_t puck_man_pos = 0;
    static int8_t puck_man_delta = 1;

    static uint16_t blinky_pos = (-2 + NUM_LEDS) % NUM_LEDS;
    static uint16_t pinky_pos  = (-3 + NUM_LEDS) % NUM_LEDS;
    static uint16_t inky_pos   = (-4 + NUM_LEDS) % NUM_LEDS;
    static uint16_t clyde_pos  = (-5 + NUM_LEDS) % NUM_LEDS;
    static uint8_t blinky_visible = 1;
    static uint8_t pinky_visible = 1;
    static uint8_t inky_visible = 1;
    static uint8_t clyde_visible = 1;
    static int8_t ghost_delta = 1;

    static uint16_t power_pellet_pos = 0;
    static bool power_pellet_flash_state = 1;
    static uint8_t puck_dots[NUM_LEDS] = {};

    static uint8_t speed_jump_cnt = 0;

    if (pattern != last_pattern_ran) {
        puck_man_pos = 0;
    }

    if (is_wait_over(draw_interval)) {
        clear();

        if (puck_man_pos == 0) {
            (*_cb)(0);
            blinky_pos = (-2 + NUM_LEDS) % NUM_LEDS;
            pinky_pos  = (-3 + NUM_LEDS) % NUM_LEDS;
            inky_pos   = (-4 + NUM_LEDS) % NUM_LEDS;
            clyde_pos  = (-5 + NUM_LEDS) % NUM_LEDS;
            blinky_visible = 1;
            pinky_visible = 1;
            inky_visible = 1;
            clyde_visible = 1;
            puck_man_delta = 1;
            ghost_delta = 1;
            speed_jump_cnt = 0;

            // the power pellet must be at least 16 leds forward of led[0]
            // from 18 to (3/4)*NUM_LEDS, multiply makes it even so that it falls on a puck_dot led
            //power_pellet_pos = 2*random16(9, (3*NUM_LEDS)/8 + 1); 
            // try different bounds for new chase speeds
            //power_pellet_pos = 2*random16(24, (3*NUM_LEDS)/8 + 1);
            power_pellet_pos = 48;

            for (uint16_t i = 0; i < NUM_LEDS; i+=2) {
                puck_dots[i] = 1;
            }
            puck_dots[power_pellet_pos] = 2;
        }

        for (uint16_t i = 0; i < NUM_LEDS; i+=2) {
            leds[(this->*dfp)(i)] = (puck_dots[i] == 1) ? CRGBA::White : CRGBA::Transparent;
        }

        if (puck_dots[power_pellet_pos] == 2) {
            if (power_pellet_flash_state) {
                power_pellet_flash_state = !power_pellet_flash_state;
                leds[(this->*dfp)(power_pellet_pos)] = CHSV(HUE_RED, 255, 255);
            }
            else {
                power_pellet_flash_state = !power_pellet_flash_state;
                leds[(this->*dfp)(power_pellet_pos)] = CRGBA::Transparent;
            }
        }

        if (puck_dots[power_pellet_pos] == 2) {
            leds[(this->*dfp)(blinky_pos)] = CHSV(HUE_RED, 255, blinky_visible*255);
            leds[(this->*dfp)(pinky_pos)]  = CHSV(HUE_PINK, 255, pinky_visible*255);
            leds[(this->*dfp)(inky_pos)]   = CHSV(HUE_AQUA, 255, inky_visible*255);
            leds[(this->*dfp)(clyde_pos)]  = CHSV(HUE_ORANGE, 255, clyde_visible*255);
        }
        else if (blinky_visible || pinky_visible || inky_visible || clyde_visible) {
            (*_cb)(1);
            puck_man_delta = -3;
            ghost_delta = -2;

            ghost_delta = -2;
            if (speed_jump_cnt < 5)
              puck_man_delta = -1;
            else if (speed_jump_cnt < 10) {
              puck_man_delta = -2;
            }
            else {
              puck_man_delta = -3;
            }
            speed_jump_cnt++;

            if (puck_man_pos == blinky_pos) {
                (*_cb)(2);
                blinky_visible = 0;
            }
            else if (puck_man_pos == pinky_pos) {
                pinky_visible = 0;
            }
            else if (puck_man_pos == inky_pos) {
                inky_visible = 0;
            }
            else if (puck_man_pos == clyde_pos) {
                clyde_visible = 0;
            }

            leds[(this->*dfp)(blinky_pos)] = CHSV(HUE_BLUE, 255, blinky_visible*255);
            leds[(this->*dfp)(pinky_pos)]  = CHSV(HUE_BLUE, 255, pinky_visible*255);
            leds[(this->*dfp)(inky_pos)]   = CHSV(HUE_BLUE, 255, inky_visible*255);
            leds[(this->*dfp)(clyde_pos)]  = CHSV(HUE_BLUE, 255, clyde_visible*255);

        }
        else {
            puck_man_delta = 1;
        }

        blinky_pos = blinky_pos + ghost_delta;
        blinky_pos = (NUM_LEDS+blinky_pos) % NUM_LEDS;
        pinky_pos = pinky_pos + ghost_delta;
        pinky_pos = (NUM_LEDS+pinky_pos) % NUM_LEDS;
        inky_pos = inky_pos + ghost_delta;
        inky_pos = (NUM_LEDS+inky_pos) % NUM_LEDS;
        clyde_pos = clyde_pos + ghost_delta;
        clyde_pos = (NUM_LEDS+clyde_pos) % NUM_LEDS;

        leds[(this->*dfp)(puck_man_pos)] = CHSV(HUE_YELLOW, 255, 255);
        puck_dots[puck_man_pos] = 0;

        puck_man_pos = puck_man_pos + puck_man_delta;
        puck_man_pos = (NUM_LEDS+puck_man_pos) % NUM_LEDS;
    }

}


// a ball will move one LED when its height changes by (2^16)/NUM_LEDS
// the fastest movement will be from led[0] to led[1] and t=0 to t=ball_time_delta
// we want the fastest movement to take one draw interval so the function isn't called unnecessarily fast
// therefore we want ball_time_delta to result in h increasing by (2^16)/NUM_LEDS
// for 52 LEDS (2^16)/NUM_LEDS = 1260 and if ball_vi = vi_max = 510
// -1*t^2 + 510*t - 1260 = 0
// minimum ball_time_delta = (-510 + sqrt(510^2 - 4*(-1*-1260))/(2*-1) = 2.48
// increasing ball_time_delta lets you increase the draw_interval therefore decreasing the frequency of redraws
void ReAnimator::bouncing_balls(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    const uint16_t vi_max = 510; // initial velocity, 512 will make h exceed UINT16_MAX
    const uint8_t blur_length = 3;
    const uint8_t num_balls = 5;
    const uint8_t ball_time_delta = 4;
    static uint16_t ball_time[num_balls] = {};
    static uint16_t ball_vi[num_balls] = {};

    if (pattern != last_pattern_ran) {
        // ??? ball_time[num_balls] = memset(ball_time, 0, sizeof(ball_time));
        // ??? ball_vi[num_balls] = memset(ball_vi, 0, sizeof(ball_vi));
        memset(ball_time, 0, sizeof(ball_time));
        memset(ball_vi, 0, sizeof(ball_vi));
    }

    if (is_wait_over(draw_interval)) {
        clear();

        for (uint8_t i = 0; i < num_balls; i++) {
            uint16_t t = ball_time[i];

            uint16_t h = (ball_vi[i] - t)*t;
            int16_t v = ball_vi[i] - 2*t;

            if (t >= ball_vi[i]) {
                // the ball has hit the ground
                h = 0;
                v = 0;
                ball_time[i] = 0;
                ball_vi[i] = random16(vi_max/3, vi_max+1); // vi_max+1 for up to and including vi_max
            }

            uint16_t pos = lerp16by16(0, NUM_LEDS-1, h);
            leds[(this->*dfp)(pos)] += CHSV(i*(256/num_balls), 255, 192);
            motion_blur((blur_length*(int32_t)v)/(int32_t)vi_max, pos, dfp);

            ball_time[i] = ball_time[i] + ball_time_delta;
        }
    }
}


void ReAnimator::halloween_colors_fade(uint16_t draw_interval) {
    CRGBPalette16 halloween_colors;
    halloween_colors = CRGBPalette16(CHSV(HUE_ORANGE, 255, 255),
                                   CHSV(HUE_PURPLE, 255, 255),
                                   CHSV(HUE_RED, 255, 255),
                                   CHSV(HUE_ALIEN_GREEN, 255, 255));

    static uint8_t delta = 0;

    if (is_wait_over(draw_interval)) {
        //fill_palette(leds, NUM_LEDS, delta, 6, halloween_colors, 255, LINEARBLEND);
        for(uint16_t i = 0; i < NUM_LEDS; i++) {
            leds[i] = ColorFromPalette(halloween_colors, delta, 255);
        }
        delta++;
    }
}


void ReAnimator::halloween_colors_orbit(uint16_t draw_interval, int8_t delta) {
    const uint8_t num_hues = 6;
    static uint8_t hi = 0;
    uint8_t hues[num_hues] = {HUE_ORANGE, HUE_PURPLE, HUE_ORANGE, HUE_RED, HUE_ORANGE, HUE_ALIEN_GREEN};

    static uint16_t pos = NUM_LEDS;

    if (pattern != last_pattern_ran) {
        pos = 0;
    }

    if (is_wait_over(draw_interval)) {
        if (delta > 0) {
            pos = pos % NUM_LEDS;
        }
        else {
            // pos underflows after it goes below zero
            if (pos > NUM_LEDS-1) {
                pos = NUM_LEDS-1;
            }
        }

        leds[pos] = CHSV(hues[hi], 255, 255);
        pos = pos + delta;
        if (pos == NUM_LEDS) {
            hi = (hi+1) % num_hues;
        }
    }
}


//void ReAnimator::sound_ribbons(uint16_t draw_interval) {
//    if (is_wait_over(draw_interval)) {
//        fadeToBlackBy(leds, NUM_LEDS, 20);
//
//        leds[NUM_LEDS/2] = CHSV(hue, 255, sound_value);
//        leds[(NUM_LEDS/2)-1] = CHSV(hue, 255, sound_value);
//
//        fission();
//    }                                                                                
//}


// derived from this code https://gist.github.com/suhajdab/9716635
//void ReAnimator::sound_ripple(uint16_t draw_interval, bool trigger) {
//    static bool enabled = true;
//    const uint16_t max_delta = 16;
//    static uint16_t delta = 0;
//    static uint16_t center = NUM_LEDS/2;
//
//    if (pattern != last_pattern_ran) {
//        enabled = true;
//        delta = 0;
//        center = NUM_LEDS/2;
//    }
//
//    if (trigger) {
//        enabled = true;
//    }
//
//    if (is_wait_over(draw_interval)) {
//        fadeToBlackBy(leds, NUM_LEDS, 170);
//
//        if (enabled) {
//            // waves created by primary droplet
//            leds[(NUM_LEDS+center+delta) % NUM_LEDS] = CHSV(hue, 255, pow(0.8, delta)*255);
//            leds[(NUM_LEDS+center-delta) % NUM_LEDS] = CHSV(hue, 255, pow(0.8, delta)*255);
//
//            if (delta > 3) {
//                // waves created by rebounded droplet
//                leds[(NUM_LEDS+center+(delta-3)) % NUM_LEDS] = CHSV(hue, 255, pow(0.8, delta - 2)*255);
//                leds[(NUM_LEDS+center-(delta-3)) % NUM_LEDS] = CHSV(hue, 255, pow(0.8, delta - 2)*255);
//            }
//
//            delta++;
//            if (delta == max_delta) {
//                delta = 0;
//                center = random16(NUM_LEDS);
//                enabled = false;
//            }
//        }
//    }
//}


//void ReAnimator::sound_orbit(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t)) {
//    if (is_wait_over(draw_interval)) {
//        for(uint16_t i = NUM_LEDS-1; i > 0; i--) {
//            leds[(this->*dfp)(i)] = leds[(this->*dfp)(i-1)];
//        }
//
//        leds[(this->*dfp)(0)] = CHSV(hue, 255, sound_value);
//    }
//}


//void ReAnimator::sound_blocks(uint16_t draw_interval, bool trigger) {
//    uint8_t hue = random8();
//
//    static bool enabled = true;
//
//    if (trigger) {
//        enabled = true;
//    }
//
//    if (is_wait_over(draw_interval)) {
//        fadeToBlackBy(leds, NUM_LEDS, 5);
//
//        if (enabled) {
//            uint16_t block_start = random16(NUM_LEDS);
//            uint8_t block_size = random8(3,8);
//            for (uint8_t i = 0; i < block_size; i++) {
//                uint16_t pos = (NUM_LEDS+block_start+i) % NUM_LEDS;
//                leds[pos] = CHSV(hue, 255, 255);
//            }
//            enabled = false;
//        }
//    }
//}


void ReAnimator::dynamic_rainbow(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    static uint16_t delta = 0;

    if (is_wait_over(draw_interval)) {
        for(uint16_t i = NUM_LEDS-1; i > 0; i--) {
            leds[(this->*dfp)(i)] = leds[(this->*dfp)(i-1)];
        }

        leds[(this->*dfp)(0)] = CHSV(((NUM_LEDS-1-delta)*255/NUM_LEDS), 255, 255);

        delta = (delta + 1) % NUM_LEDS;
    }
}


// ++++++++++++++++++++++++++++++
// ++++++++++ OVERLAYS ++++++++++
// ++++++++++++++++++++++++++++++

void ReAnimator::breathing(uint16_t interval) {
    const uint8_t min_brightness = 2;
    static uint8_t delta = 0; // goes up to 255 then overflows back to 0

    if (finished_waiting(interval)) {
        // since FastLED is managing the maximum power delivered use the following function to find the _actual_ maximum brightness allowed for
        // these power consumption settings. setting brightness to a value higher that max_brightness will not actually increase the brightness.
        //uint8_t max_brightness = calculate_max_brightness_for_power_vmA(leds, NUM_LEDS, homogenized_brightness, LED_STRIP_VOLTAGE, selected_led_strip_milliamps);
        uint8_t max_brightness = 128;
        uint8_t b = scale8(triwave8(delta), max_brightness-min_brightness)+min_brightness;

        DEBUG_PRINTLN(b);
        FastLED.setBrightness(b);

        delta++;
    }
}


void ReAnimator::flicker(uint16_t interval) {
    fade_randomly(10, 150);

    // an on or off period less than 16 ms probably can't be perceived
    if (finished_waiting(interval)) {
        //FastLED.setBrightness((random8(1,11) > 4)*homogenized_brightness);
        uint8_t r = (random8(1,11) > 4);
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            leds[i].fadeToBlackBy(r*homogenized_brightness);
        }
    }
}


void ReAnimator::glitter(uint16_t chance_of_glitter) {
    if (chance_of_glitter > random16()) {
        leds[random16(NUM_LEDS)] += CRGBA::White;
    }
}


void ReAnimator::fade_randomly(uint8_t chance_of_fade, uint8_t decay) {
    for (uint16_t i = 0; i < NUM_LEDS; i++) {
        if (chance_of_fade > random8()) {
            leds[i].fadeToBlackBy(decay);
        }
    }
}


// ++++++++++++++++++++++++++++++
// +++++++++++ IMAGE ++++++++++++
// ++++++++++++++++++++++++++++++
/*
bool ReAnimator::load_image_from_json(String json, String* message) {
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



// ++++++++++++++++++++++++++++++
// ++++++++++++ TEXT ++++++++++++
// ++++++++++++++++++++++++++++++
uint8_t ReAnimator::get_text_center(String s) {
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
            min_row = min(j, min_row);
            max_row = max(j, max_row);
            break;
          }
        }
      }
    }
  }

  if ((max_row-min_row+1)/2 + min_row > 0) {
    return (max_row-min_row+1)/2 + min_row;
  }
  return 0;
}


/*
// do not delete these yet. they might be better for use with position functions.
void ReAnimator::matrix_char(char c) {
  const uint8_t* glyph = font->get_bitmap(font, c);
  if (glyph) {
    for (uint16_t q = 0; q < NUM_LEDS; q++) {
      leds[q] = CHSV(0, 0, 0);
    }
    uint8_t width = font->get_width(font, c);
    uint16_t n = 0;
    uint8_t pad = (MD-width)/2;
    for (uint8_t i = 0; i < MD; i++) {
      for (uint8_t j = 0; j < width; j++) {
        uint8_t k = (MD-1)-pad-j;
        Point p;
        p.x = k;
        p.y = i;
        leds[cart2serp(p)] = CHSV(hue, 255, glyph[n]);
        n++;
      }
    }
  }
}

void ReAnimator::matrix_text(String s) {
  static uint32_t pm = 0;
  static uint16_t i = 0;
  if ((millis()-pm) > 1000) {
    pm = millis();
    matrix_char(s[i]);
    i = (i+1) % s.length();
  }
}
*/


bool ReAnimator::shift_char(char c, int8_t vmargin) {
  if (shift_char_tracking) {
    for (uint8_t i = 0; i < MD; i++) {
      for (uint8_t j = 0; j < MD-1; j++) {
        uint8_t k = (MD-1)-j;
        Point p1;
        Point p2;
        p1.x = k;
        p1.y = i;
        p2.x = k-1;
        p2.y = i;
        leds[cart2serp(p1)] = leds[cart2serp(p2)];
      }
      Point p;
      p.x = 0;
      p.y = i;
      leds[cart2serp(p)] = 0x00000000; // transparent black

    }
    shift_char_tracking--;
    return false;
  }

  bool finished_shifting = false;
  uint8_t width = 0;

  const uint8_t* glyph = font->get_bitmap(font, c);
  if (glyph != nullptr) {
    width = font->get_width(font, c);
    for (uint8_t i = 0; i < MD; i++) {
      for (uint8_t j = 0; j < MD-1; j++) {
        uint8_t k = (MD-1)-j;
        Point p1;
        Point p2;
        p1.x = k;
        p1.y = i;
        p2.x = k-1;
        p2.y = i;
        leds[cart2serp(p1)] = leds[cart2serp(p2)];
      }
      Point p;
      p.x = 0;
      p.y = i;

      uint16_t n = (i-vmargin)*width + shift_char_column;
      if (0 <= n && n < width*font->h_px) {
        CRGB pixel = *rgb;
        //pixel = pixel.scale8(glyph[n]); // not all glyph pixels are completely off or on, so dim for those in between.
        leds[cart2serp(p)] = pixel;
        //leds[cart2serp(p)].a = (glyph[n] == 0) ? 0 : 255; // remove partial transparency
        leds[cart2serp(p)].a = glyph[n];
      }
      else {
        leds[cart2serp(p)] = 0x00000000; // transparent black
      }
    }

    shift_char_column = (shift_char_column+1)%width;
    if (shift_char_column == 0) {
      finished_shifting = true;  // character fully shifted onto matrix.
      shift_char_tracking = 1; // add tracking (spacing between letters) on next call
    }
  }

  return finished_shifting;
}



// ++++++++++++++++++++++++++++++
// ++++++++++++ INFO ++++++++++++
// ++++++++++++++++++++++++++++++
void ReAnimator::setup_clock() {
  configTzTime(timezone, "pool.ntp.org");
}


void ReAnimator::refresh_date_time(uint16_t draw_interval) {
    const uint8_t nd = 4;
    const Point corners[nd] = {{.x = MD-1-3, .y = 0}, {.x = MD-1-9, .y = 0}, {.x = MD-1-3, .y = 8}, {.x = MD-1-9, .y = 8}};

    if (is_wait_over(draw_interval)) {
        struct tm local_now = {0};
        // the second argument of getLocalTime indicates how long it should loop waiting for the time to be received from ntp
        // since getLocalTime() is already being called around every 200 ms (by the if above) there is no need for getLocalTime()
        // to loop, so set the second argument to 0. using 0 also keeps the other animations from being getLocalTime() looping.
        // calling it like this does not spam the ntp server.
        if (getLocalTime(&local_now, 0)) {

            char ts[nd+1];
            //char ts[nd+1] = {'0', '6', '1', '2', '\0'};
            if (_id == 0 || _id == 1 || ((_id == 4 || _id == 5) && local_now.tm_sec % 7 != 0)) {
                uint8_t hour = ((_id == 0 || _id == 4) && local_now.tm_hour > 12) ? local_now.tm_hour-12 : local_now.tm_hour;
                snprintf(ts, sizeof ts, "%02d%02d", hour, local_now.tm_min);
            }
            else if (_id == 2 || _id == 3 || ((_id == 4 || _id == 5) && local_now.tm_sec % 7 == 0)) {
                if (_id == 2 || _id == 4) {
                    snprintf(ts, sizeof ts, "%02d%02d", local_now.tm_mon+1, local_now.tm_mday);
                }
                else if (_id == 3 || _id == 5) {
                    snprintf(ts, sizeof ts, "%02d%02d", local_now.tm_mday, local_now.tm_mon+1);
                }
            }

            for (uint8_t i = 0; i < nd; i++) {
                char c = ts[i];
                const uint8_t* glyph = clkfont->get_bitmap(clkfont, c);
                uint8_t height = clkfont->h_px;
                uint8_t width = clkfont->get_width(clkfont, c);
                if (glyph != nullptr) {
                    uint8_t n = 0;
                    Point p0 = corners[i];
                    for (uint8_t j = 0; j < height; j++) {
                        for (uint8_t k = 0; k < width; k++) {
                            uint8_t kk = p0.x-k;
                            Point p;
                            p.x = kk;
                            p.y = p0.y+j;

                            // there are a couple of different ways you can show a glyph
                            // 1) you can use scale8 to dim the pixel's color such that the negative space becomes black.
                            //    without being combined with transparency this will result in blocky characters, and black text is not possible.
                            //    CRGB pixel = *rgb;
                            //    pixel = pixel.scale8(glyph[n]);
                            //    leds[cart2serp(p)] = pixel;
                            // 2) you can set the alpha such that negative space is completely transparent
                            //    characters are not blocky because negative space is invisible, and black text is possible
                            //    CRGB pixel = *rgb;
                            //    leds[cart2serp(p)] = pixel;
                            //    leds[cart2serp(p)].a = glyph[n];

                            CRGB pixel = *rgb;
                            // setting color to black is not necessary when transparency is used but it may be advantageous in a yet
                            // unknow way, so keeping this line but commenting it out.
                            //pixel = pixel.scale8(glyph[n]); // not all glyph pixels are completely off or on, so dim for those in between.
                            leds[cart2serp(p)] = pixel;
                            //leds[cart2serp(p)].a = (glyph[n] == 0) ? 0 : 255; // remove partial transparency
                            leds[cart2serp(p)].a = glyph[n];

                            //char dc = (glyph[n] == 0) ? '.' : '#';
                            //Serial.print(dc);
                            n++;
                        }
                        //Serial.println("");
                    }
                }
            }
        }
    }
}


// ++++++++++++++++++++++++++++++
// ++++++++++ CONTROL +++++++++++
// ++++++++++++++++++++++++++++++
// loop through all of the patterns
void ReAnimator::autocycle() {
    if((millis() - autocycle_previous_millis) > autocycle_interval) {
        autocycle_previous_millis = millis();
        DEBUG_PRINTLN("autocycle started");
        if (increment_pattern(false) == INT8_MIN) {
            // autocycle has looped back around to the first pattern so reverse them
            reverse = !reverse;
        }
    }
}

// alternate between running a pattern forwards or backwards
void ReAnimator::flipflop() {
    if((millis() - flipflop_previous_millis) > flipflop_interval) {
        flipflop_previous_millis = millis();
        DEBUG_PRINTLN("flip flop loop started");
        reverse = !reverse;
    }
}




// ++++++++++++++++++++++++++++++
// ++++++++ POSITIONING +++++++++
// ++++++++++++++++++++++++++++++
ReAnimator::Point ReAnimator::serp2cart(uint8_t i) {
  const uint8_t rl = 16;
  Point p;
  p.y = i/rl;
  p.x = (p.y % 2) ? (rl-1) - (i % rl) : i % rl;
  return p;
}


int16_t ReAnimator::cart2serp(Point p) {
  const uint8_t rl = 16;
  int16_t i = (p.y % 2) ? (rl*p.y + rl-1) - p.x : rl*p.y + p.x;
  return i;
}


void ReAnimator::flip(CRGB sm[NUM_LEDS], bool dim) {
  Point p1;
  Point p2;
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
uint16_t ReAnimator::translate(uint16_t i, int8_t xi, int8_t yi, int8_t sx, int8_t sy, bool wrap, int8_t gap) {
  // the origin of the matrix is in the NORTHEAST corner, so positive moves head WEST and SOUTH
  // this function's logic treats entering the matrix from the EAST border (WESTWARD movement) or NORTH border (SOUTHWARD movement)
  // as the basic case and flips those to get EAST and NORTH.
  // this approach simplifies the code because d and v will be positive (after transient period) 
  // which lets us just use mod to wrap the output instead of having to account for wrapping around
  // when d is positive or negative.

  if (t_initial) {
    t_initial = false;
    dx = (xi >= MD) ? -abs(xi) : xi;
    dy = (yi >= MD) ? -abs(yi) : yi;
  }

  uint16_t ti = NUM_LEDS; // used to indicate pixel is not in bounds and should not be drawn.

  Point p1 = serp2cart(i);
  Point p2;

  if (t_has_entered && !wrap && !t_visible) {
    // wrapping is not turned on and input has passed through matrix never to return
    return ti;
  }

  t_visible = false;
  gap = (gap == -1) ? MD : gap; // if gap is -1 set gap to MD so that the input only appears in one place but will still loop around

  uint8_t ux = (sx > 0) ? MD-1-p1.x: p1.x; // flip heading output travels
  uint8_t uy = (sy > 0) ? MD-1-p1.y: p1.y;

  int8_t vx = ux+dx; // shift input over into output by d
  int8_t vy = uy+dy;
  if (t_has_entered && wrap) {
    vx = vx % (MD+gap);
    vy = vy % (MD+gap);
  }

  if ( 0 <= vx && vx < MD && 0 <= vy && vy < MD ) {
    t_visible = true;
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
      t_has_entered = true;
    }

    if (t_has_entered && wrap) {
        dx = dx % (MD+gap);
        dy = dy % (MD+gap);
    }
  }
  return ti;
}


void ReAnimator::ntranslate(CRGBA in[NUM_LEDS], CRGBA out[NUM_LEDS], int8_t xi, int8_t yi, int8_t sx, int8_t sy, bool wrap, int8_t gap) {
// an output array is required to use this function. out[] should be what is displayed on the matrix.
  if (t_initial) {
    t_initial = false;
    t_visible = false;
    t_has_entered = false;
    dx = (xi >= MD) ? -abs(xi) : xi;
    dy = (yi >= MD) ? -abs(yi) : yi;
  }

  Point p1;
  Point p2;

  if (t_has_entered && !wrap && !t_visible) {
    // wrapping is not turned on and input has passed through matrix never to return
    return;
  }

  t_visible = false;
  gap = (gap == -1) ? MD : gap; // if gap is -1 set gap to MD so that the input only appears in one place but will still loop around

  for (uint8_t j = 0; j < MD; j++) {
    for (uint8_t k = 0; k < MD; k++) {
      uint8_t ux = (sx > 0) ? MD-1-k: k; // flip heading output travels
      uint8_t uy = (sy > 0) ? MD-1-j: j;

      p1.x = ux;
      p1.y = uy;
      out[cart2serp(p1)] = CRGB::Black; // ??? TRANSPARENT 0x424242

      int8_t vx = k+dx; // shift input over into output by d
      int8_t vy = j+dy;
      if (t_has_entered && wrap) {
        vx = vx % (MD+gap);
        vy = vy % (MD+gap);
      }

      if ( 0 <= vx && vx < MD && 0 <= vy && vy < MD ) {
        t_visible = true;
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
    t_has_entered = true;
  }

  if (t_has_entered && wrap) {
      dx = dx % (MD+gap);
      dy = dy % (MD+gap);
  }
}

uint16_t ReAnimator::mover(uint16_t i) {
  uint16_t ti;
  switch (heading) {
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



// ++++++++++++++++++++++++++++++
// ++++++++++ HELPERS +++++++++++
// ++++++++++++++++++++++++++++++
//void ReAnimator::fadeToBlackBy(CRGBA pixel, uint8_t fadeBy) {
//    CRGB pixel_rgb = (CRGB)pixel;
//    pixel_rgb = pixel_rgb.scale8(255-fadeBy);
//}

void ReAnimator::fadeToBlackBy(CRGBA* leds, uint16_t num_leds, uint8_t fadeBy) {
  for (uint16_t i = 0; i < num_leds; i++) {
    leds[i].fadeToBlackBy(fadeBy);
  }
}

void ReAnimator::fill_solid( struct CRGBA * targetArray, int numToFill, const struct CRGB& color) {
    for( int i = 0; i < numToFill; ++i) {
        targetArray[i] = color;
    }
}


uint16_t ReAnimator::forwards(uint16_t index) {
    return index;
}


uint16_t ReAnimator::backwards(uint16_t index) {
    return (NUM_LEDS-1)-index;
}


// If two functions running close to each other both call is_wait_over()
// the one with the shorter interval will reset the timer such that the
// function with the longer interval will never see its interval has
// elapsed, therefore a second function that does the same thing as
// is_wait_over() has been added. This is only a concern when a pattern
// function and an overlay function are both called at the same time.
// Patterns should use is_wait_over() and overlays should use finished_waiting(). 
bool ReAnimator::is_wait_over(uint16_t interval) {
    //static uint32_t iwopm = 0; // previous millis
    if ( (millis() - iwopm) > interval ) {
        iwopm = millis();
        return true;
    }
    else {
        return false;
    }
}


bool ReAnimator::finished_waiting(uint16_t interval) {
    //static uint32_t fwpm = 0; // previous millis
    if ( (millis() - fwpm) > interval ) {
        fwpm = millis();
        return true;
    }
    else {
        return false;
    }
}


void ReAnimator::accelerate_decelerate_pattern(uint16_t draw_interval_initial, uint16_t delta_initial, uint16_t update_period, uint16_t genparam, void(ReAnimator::*pfp)(uint16_t, uint16_t, uint16_t(ReAnimator::*dfp)(uint16_t)), uint16_t(ReAnimator::*dfp)(uint16_t)) {
    static uint16_t draw_interval = draw_interval_initial;
    static int8_t delta = delta_initial;

    if (pattern != last_pattern_ran) {
        draw_interval = draw_interval_initial;
        delta = delta_initial;
    }

    if (finished_waiting(update_period)) {
        draw_interval = draw_interval - delta;
        if (draw_interval <= 0 || draw_interval >= draw_interval_initial) {
            delta = -1*delta;
        }
    }

    (this->*pfp)(draw_interval, genparam, dfp);
}


// derived from this code https://github.com/atuline/FastLED-Demos/blob/master/soundmems_demo/soundmems.h
//void ReAnimator::process_sound() {
//    const uint16_t DC_OFFSET = 513;  // measured
//    const uint8_t NUM_SAMPLES = 64;
//
//    static int16_t sample_buffer[NUM_SAMPLES];
//    static uint16_t sample_sum = 0;
//    static uint8_t i = 0;
//
//    int16_t sample = 0;
//
//    sample_peak = 0;
//
//    sample = analogRead(MIC_PIN) - DC_OFFSET;
//    sample = abs(sample);
//
//    if (sample < sample_threshold) {
//        sample = 0;
//    }
//
//    sample_sum += sample - sample_buffer[i]; // add newest sample and subtract oldest sample from the sum
//    sample_average = sample_sum / NUM_SAMPLES;
//    sample_buffer[i] = sample;  // overwrite oldest sample with newest sample
//    i = (i + 1) % NUM_SAMPLES;
//
//    sound_value = sound_value_gain*sample_average;
//    sound_value = min(sound_value, 255);
//
//    if (sample > (sample_average + sample_threshold) && (sample < previous_sample)) {
//        sample_peak = 1;
//    }
//  
//    previous_sample = sample;
//}


void ReAnimator::motion_blur(int8_t blur_num, uint16_t pos, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    if (blur_num > 0) {
        for (uint8_t i = 1; i < blur_num+1; i++) {
            if (pos >= pos-i) {
                leds[(this->*dfp)(pos-i)] += leds[(this->*dfp)(pos)];
                leds[(this->*dfp)(pos-i)].fadeToBlackBy(120+(i*120/blur_num));
            }
        }
    }
    else if (blur_num < 0) {
        for (uint8_t i = 1; i < abs(blur_num)+1; i++) {
            if (pos+i < NUM_LEDS) {
                leds[(this->*dfp)(pos+i)] += leds[(this->*dfp)(pos)];
                leds[(this->*dfp)(pos+i)].fadeToBlackBy(120+(i*120/abs(blur_num)));
            }
        }
    }
}


void ReAnimator::fission() {
    for (uint16_t i = NUM_LEDS-1; i > NUM_LEDS/2; i--) {
        leds[i] = leds[i-1];
    }

    for (uint16_t i = 0; i < NUM_LEDS/2; i++) {
        leds[i] = leds[i+1];
    }
}


// freeze_interval must be greater than m_failsafe_timeout
void ReAnimator::Freezer::timer(uint16_t freeze_interval) {
    static uint32_t pm = millis() - freeze_interval; // previous millis

    if ((millis() - pm) > freeze_interval) {
        pm = millis();
        m_frozen = true;
        m_frozen_previous_millis = millis();
    }
}


bool ReAnimator::Freezer::is_frozen() {
    static bool all_black = false;
    static uint16_t frozen_duration = m_failsafe_timeout;

    if ((millis() - m_frozen_previous_millis) > frozen_duration) {
        m_frozen = false;
        all_black = false;
        frozen_duration = m_failsafe_timeout;
    }
    else if (!all_black) {
        for (uint16_t i = 0; i < NUM_LEDS; i++) {
            all_black = true;
            if (parent.leds[i] != CRGBA::Transparent) {
                all_black = false;
                break;
            }
        }
        if (all_black && ((m_frozen_previous_millis + m_failsafe_timeout) - millis()) > m_after_all_black_pause) {
            // after all the LEDs after found to be dark unfreeze after a short pause
            m_frozen_previous_millis = millis();
            frozen_duration = m_after_all_black_pause;
        }
    }

    return m_frozen;
}


// ??? static int ReAnimator::compare(const void *a, const void *b) {
int ReAnimator::compare(const void *a, const void *b) {
  Starship *StarshipA = (Starship *)a;
  Starship *StarshipB = (Starship *)b;

  return (StarshipB->distance > StarshipA->distance) - (StarshipA->distance > StarshipB->distance); // descending order
}


/*
void ReAnimator::print_dt() {
    static uint32_t pm = 0; // previous millis
    Serial.print("dt: ");
    Serial.println(millis() - pm);
    pm = millis();
}
*/









/*
void demo() {
  const uint8_t PATTERNS_NUM = 21;

  const Pattern patterns[PATTERNS_NUM] = {DYNAMIC_RAINBOW, SOLID, ORBIT, RUNNING_LIGHTS,
                                          JUGGLE, SPARKLE, WEAVE, CHECKERBOARD, BINARY_SYSTEM,
                                          SOLID, SOLID, SOLID, SOLID, DYNAMIC_RAINBOW,
                                          SHOOTING_STAR, //MITOSIS, BUBBLES, MATRIX,
                                          BALLS, CYLON,
                                          //STARSHIP_RACE
                                          //PUCK_MAN, // PUCK_MAN crashes ???
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

