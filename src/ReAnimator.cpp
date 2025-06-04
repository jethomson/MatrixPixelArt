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
#include <StreamUtils.h>


// Conventions
// -----------
// Forward means a directional pattern moves away from pixel 0 and toward the last pixel in the strip.
// Backward means a directional pattern moves toward pixel 0 and away from the last pixel in the strip.
// If LEFT_TO_RIGHT_IS_FORWARD is true then pixel 0 is the leftmost pixel as seen by the viewer (i.e. the viewer's left),
// pushing a right arrow button moves a pattern forward from left to right, and pushing a left arrow button moves a 
// pattern backward from right to left.
// If LEFT_TO_RIGHT_IS_FORWARD is false then pixel 0 is the rightmost pixel as seen by the viewer (i.e. the viewer's right),
// pushing a right arrow button moves a pattern backward from left to right, and pushing a left arrow button moves a 
// pattern forward from right to left.
#define LEFT_TO_RIGHT_IS_FORWARD true

//LV_FONT_DECLARE(ascii_sector_12); //OR use extern lv_font_t ascii_sector_12;
extern lv_font_t ascii_sector_12;

//const char* timezone = "EST5EDT,M3.2.0,M11.1.0";

// set queue size to NUM_LAYERS+1 because every layer of a composite could be an image, with the +1 for a little safety margin
// since xQueueSend has xTicksToWait set to 0 (i.e. do not wait for empty queue slot if queue is full).
QueueHandle_t ReAnimator::qimages = xQueueCreate(NUM_LAYERS+1, sizeof(Image));

inline void cb_dbg_print(uint32_t i) {
  DEBUG_PRINTLN(i);
}

ReAnimator::ReAnimator(uint8_t num_rows, uint8_t num_cols) : freezer(*this) {
    set_lvfmcb(&cb_dbg_print);

    // it should be possible to use ReAnimator with matrices of different sizes in the same project
    // so store matrix size in the object instead of making it static to the class
    MTX_NUM_ROWS = num_rows,
    MTX_NUM_COLS = num_cols;
    MTX_NUM_LEDS = num_rows*num_cols;

    // abort() is called if out of memory so no point in trying to check?
    leds = new CRGBA[MTX_NUM_LEDS];

    _ltype = static_cast<LayerType>(-1);
    _id = -1;

    image_path = "";

    clear(); // initialize leds[]

    layer_brightness = 255;

    autocycle_previous_millis = 0;
    autocycle_interval = 10000;
    autocycle_enabled = false;

    flipflop_previous_millis = 0;
    flipflop_interval = 6000;
    flipflop_enabled = false;

    pattern = NO_PATTERN;
    last_pattern = NO_PATTERN;
    transient_overlay = NO_OVERLAY;
    persistent_overlay = NO_OVERLAY;

    // direction indicates why point the leading pixel of a pattern advances toward: end of strip or beginning of strip.
#if !defined(LEFT_TO_RIGHT_IS_FORWARD) || LEFT_TO_RIGHT_IS_FORWARD
    direction_fp = &ReAnimator::forwards;
    antidirection_fp = &ReAnimator::backwards;
#else
    direction_fp = &ReAnimator::backwards;
    antidirection_fp = &ReAnimator::forwards;
#endif
    reverse = false;

    // using yellow can help spot errors
    internal_rgb = CHSV(HUE_YELLOW, 255, 255);
    rgb = &internal_rgb;
    hue = HUE_YELLOW;
    proxy_color_set = false;

    _cb = noop_cb;

    // puck-man variables
    pm_puck_man_pos = 0;
    pm_puck_man_delta = 1;
    pm_blinky_pos = (-2 + MTX_NUM_LEDS) % MTX_NUM_LEDS;
    pm_pinky_pos  = (-3 + MTX_NUM_LEDS) % MTX_NUM_LEDS;
    pm_inky_pos   = (-4 + MTX_NUM_LEDS) % MTX_NUM_LEDS;
    pm_clyde_pos  = (-5 + MTX_NUM_LEDS) % MTX_NUM_LEDS;
    pm_blinky_visible = 1;
    pm_pinky_visible = 1;
    pm_inky_visible = 1;
    pm_clyde_visible = 1;
    pm_ghost_delta = 1;
    pm_power_pellet_pos = 0;
    pm_power_pellet_flash_state = 1;
    //pm_puck_dots = (uint8_t*)malloc(MTX_NUM_LEDS*sizeof(uint8_t));
    pm_puck_dots = new uint8_t[MTX_NUM_LEDS];
    for (uint16_t i = 0; i < MTX_NUM_LEDS; i++) {
      pm_puck_dots[i] = 0;
    }

    pm_speed_jump_cnt = 0;

    dr_delta = 0;
    o_pos = MTX_NUM_LEDS;
    gc_delta = 0;
    rl_delta = 0;
    ss_start_pos = random16(0, MTX_NUM_LEDS/4);
    ss_stop_pos = random16(MTX_NUM_LEDS/2, MTX_NUM_LEDS);
    ss_cdi_pm = 0; // cool_down_interval_previous_millis
    ss_pos = 0;
    c_pos = 0;
    c_delta = 1;
    p_t = 0;
    f_t = 0;
    r_t = 0;
    w_pos = 0;
    adp_draw_interval = 0;
    adp_delta = 0;

    image_dequeued = false;
    image_loaded = false;
    image_clean = false;
    image_queued_time = millis();
    display_duration = REFRESH_INTERVAL;

    font = &ascii_sector_12;

    refresh_text_index = 0;
    shift_char_column = 0;
    shift_char_tracking = 0; // spacing between letters

    // heading indicates which direction an object (image, patter, text, etc.) displayed on the matrix moves
    heading = 0;
    t_initial = true;
    t_visible = false;
    t_has_entered = false;
    dx = 0;
    dy = 0;

    iwopm = millis(); // previous millis for is_wait_over()
    fwpm = millis(); // previous millis for finished_waiting()
}


ReAnimator::Freezer::Freezer(ReAnimator &r) : parent(r) {
    m_frozen = false;
    m_frozen_previous_millis = 0;
}


void ReAnimator::setup(LayerType ltype, int8_t id) {
    layer_brightness = 255;
    proxy_color_set = false;

    // we want patterns to persist from one composite to another if they are on the same layer.
    // this allows for a pattern to play continuously (i.e without restarting) when the next item in a playlist is loaded.
    //
    // scrolling text uses an id of -1 because we always want to clear() when new text is set.
    // images use an id of -2 because real image ids (paths) are more complicated and it is unnecessary to clear since a new image should completely overwrite leds[]
    // in practice there is probably no observable difference between -2 and -1 for images.
    // could possibly get an interesting effect with -2 if the new image does not write to all of leds[] and part of the previous image remains.
    if (id == -1 || !(_ltype == ltype && _id == id)) {
        _ltype = ltype;
        _id = id;

        // since layers are reused the remnants of the old effect may still be in leds[]
        // these leftovers may not be overwritten by the new effect, so it is best to clear leds[]
        clear();

        set_autocycle_interval(10000);
        set_autocycle_enabled(false);
        set_flipflop_interval(6000);
        set_flipflop_enabled(false);

        set_pattern(NO_PATTERN);
        set_overlay(NO_OVERLAY, false);
        set_cb(noop_cb);

        image_dequeued = false;
        image_loaded = false;
        image_clean = false;
        image_queued_time = millis();
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

    if (autocycle_enabled && pattern_in == NO_PATTERN) {
        pattern_in = static_cast<Pattern>(pattern+1);
    }

    switch(pattern_in) {
        default:
            retval = INT8_MIN;
            // fall through to next case
        case DYNAMIC_RAINBOW:
            pattern_out = DYNAMIC_RAINBOW;
            overlay_out = NO_OVERLAY;
            break;
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
        case PENDULUM:
            pattern_out = PENDULUM;
            overlay_out = NO_OVERLAY;
            break;
        case FUNKY:
            pattern_out = FUNKY;
            overlay_out = NO_OVERLAY;
            break;
        case RIFFLE:
            pattern_out = RIFFLE;
            overlay_out = NO_OVERLAY;
            break;
        case SPARKLE:
            pattern_out = SPARKLE;
            overlay_out = NO_OVERLAY;
            break;
        case WEAVE:
            pattern_out = WEAVE;
            overlay_out = NO_OVERLAY;
            break;
        case PUCK_MAN:
            pattern_out = PUCK_MAN;
            overlay_out = NO_OVERLAY;
            break;
        case RAIN:
            pattern_out = RAIN;
            overlay_out = NO_OVERLAY;
            break;
        case WATERFALL:
            pattern_out = WATERFALL;
            overlay_out = NO_OVERLAY;
            break;
        case NO_PATTERN:
            pattern_out = NO_PATTERN;
            overlay_out = NO_OVERLAY;
            break;
    }

    last_pattern = pattern;
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
        case BREATHING:
            overlay_out = BREATHING;
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


void ReAnimator::set_image(String id, uint32_t duration, String* message) {
    image_path = form_path(F("im"), id);
    image_dequeued = false;
    image_loaded = false;
    image_clean = false;
    image_queued_time = millis();
    display_duration = duration;
    //xQueueSend makes a copy of image, so it is OK that image is a local variable.
    Image image = {&image_path, &MTX_NUM_LEDS, leds, &proxy_color_set, &proxy_color, &image_dequeued, &image_loaded, &image_clean};
    xQueueSend(qimages, (void *)&image, 0);
}


// this runs on core 0
// loading an image takes a while which can make the animation laggy if ran on the same core as the main code
void ReAnimator::load_image_from_queue(void* parameter) {
    for (;;) {
        // calling vTaskDelay() prevents watchdog error, but I'm not sure why and if this is a good way to handle it  
        vTaskDelay(pdMS_TO_TICKS(1)); // 1 ms
        Image image;
        if (xQueueReceive(qimages, (void *)&image, 0) == pdTRUE) {
            //print_dt();

            // cannot set image_dequeued here because it may be read before a proper value for image_loaded is determined
            //*(image.image_dequeued) = true;

            // make sure we are not referencing leds in a layer that was destroyed
            if (image.leds == nullptr) {
                *(image.image_loaded) = false;
                *(image.image_clean) = false;
                *(image.image_dequeued) = true;
                continue;
            }

            if (*(image.image_path) == "") {
                *(image.image_loaded) = false;
                *(image.image_clean) = false;
                *(image.image_dequeued) = true;
                continue;
            }

            File file = LittleFS.open(*(image.image_path), "r");
            if (!file) {
                *(image.image_loaded) = false;
                *(image.image_clean) = false;
                *(image.image_dequeued) = true;
                continue;
            }

            if (file.available()) {
                DynamicJsonDocument doc(8192);
                ReadBufferingStream bufferedFile(file, 64);
                DeserializationError error = deserializeJson(doc, bufferedFile);
                if (error) {
                    *(image.image_loaded) = false;
                    *(image.image_clean) = false;
                    *(image.image_dequeued) = true;
                    continue;
                }

                JsonObject object = doc.as<JsonObject>();

                *(image.proxy_color_set) = false;
                if (!object[F("pc")].isNull()) {
                    std::string pc = object[F("pc")].as<std::string>();
                    if (!pc.empty()) {
                        *(image.proxy_color) = std::stoul(pc, nullptr, 16);
                        *(image.proxy_color_set) = true;
                    }
                }

                // for unknown reasons initializing the leds[] to all black
                // makes the code slightly faster
                for (uint16_t i = 0; i < *(image.MTX_NUM_LEDS); i++) image.leds[i] = CRGBA::Transparent;
                *(image.image_loaded) = deserializeSegment(object, image.leds, *(image.MTX_NUM_LEDS));
                *(image.image_clean) = *(image.image_loaded);
                *(image.image_dequeued) = true;
            }
            file.close();
            //print_dt();
        }
    }
    vTaskDelete(NULL);
}


int8_t ReAnimator::get_image_status() {
    // if image was never queued this code will not work correctly, image will never be determined to be broken.
    if (image_dequeued) {
        if (image_loaded) {
            return 1; // image loaded successfully into leds[]
        }
        DEBUG_PRINTLN("bad image");
        return -1; // image is broken
    }
    return 0; // still waiting for image to be dequeued
}


void ReAnimator::set_text(String s) {
    // need to reinitialize when one string was already being written and new string is set
    refresh_text_index = 0; // start at the beginning of a string
    shift_char_column = 0; // start at the beginning of a glyph

    ftext.s = s;

    int16_t max_height_above_baseline = 0;
    for (uint8_t i = 0; i < s.length(); i++) {
        char c = s[i];
        uint16_t box_h = 0;
        int16_t offset_y = 0;

        const uint8_t* glyph = get_bitmap(font, s[i], '\0', nullptr, nullptr, &box_h, &offset_y);
        if (glyph && box_h > 0) {
            max_height_above_baseline = max((int16_t)(box_h+offset_y), max_height_above_baseline);
            ftext.baseline = max((int16_t)(-offset_y), ftext.baseline);
        }
    }
    ftext.line_height = max_height_above_baseline+ftext.baseline;
    ftext.vmargin = (MTX_NUM_ROWS - ftext.line_height)/2;
}


void ReAnimator::set_info(Info type) {
  _id = type;
  //switch(type) {
  //  default:
  //    // fall through to next case
  //    case TIME_12HR:
  //    case TIME_24HR:
  //    case DATE_MMDD:
  //    case DATE_DDMM:
  //    case TIME_12HR_DATE_MMDD:
  //    case TIME_24HR_DATE_DDMM:
  //      break;
  //    //case 6:
  //      // maybe do scrolling count down here
  //      //break;
  //    //case 7:
  //      // maybe do weather info
  //      //break;
  //}
}


// the frontend color picker is focused on RGB so try to honor the RGB color picked.
// sometimes it makes more sense to work with a hue, so we have a hue variable that is derived from rgb.
// color can be determined externally (*rgb) or internally (internal_rgb) to the layer.
// if internal_rgb is used then rgb is made to point to that.
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


void ReAnimator::clear() {
    for (uint16_t i = 0; i < MTX_NUM_LEDS; i++) {
        leds[i] = CRGBA::Transparent;
    }
}


void ReAnimator::reanimate() {
    // some patterns rely on hue. if rgb is dynamic, it is ever changing, so hue has to be updated to reflect the current rgb value.
    CHSV chsv = rgb2hsv_approximate(*rgb);
    hue = chsv.h; // !!BUG!! if color is 0x000000 (black) then hue will be 0 which is red when CHSV(hue, 255, 255)

    if (autocycle_enabled) {
        autocycle();
    }

    if (flipflop_enabled) {
        flipflop();
    }

    if (!freezer.is_frozen()) {
        if (_ltype == Image_t && image_loaded && !image_clean) {
            // frozen_decay changed image, so refresh the image.
            image_dequeued = false;
            image_loaded = false;
            image_clean = false;
            Image image = {&image_path, &MTX_NUM_LEDS, leds, &proxy_color_set, &proxy_color, &image_dequeued, &image_loaded, &image_clean};
            xQueueSend(qimages, (void *)&image, 0);
        }
        else if (_ltype == Pattern_t) {
            run_pattern(pattern);
            last_pattern = pattern;
        }
        else if (_ltype == Text_t) {
            refresh_text(200);
        }
        else if (_ltype == Info_t) {
            refresh_info(200);
        }
    }

    apply_overlay(transient_overlay);
    apply_overlay(persistent_overlay);

    //print_dt();
}


CRGBA ReAnimator::get_pixel(uint16_t i) {
    //CRGBA pixel_out = 0xFF000000; // if black with no transparency is used it creates a sort of spotlight effect
    CRGBA pixel_out = CRGBA::Transparent;
    uint16_t ti = mover(i);
    if (0 <= ti && ti < MTX_NUM_LEDS) {
        pixel_out = leds[ti];

        // color substitution
        // some browsers slightly modify the RGB values of the canvas to prevent tracking. Brave calls this farbling.
        // this means the values sent from the converter page do not have the exact same RGB values as the source image.
        // this if condition accepts values that are similar to the proxy_color.
        //if ( _ltype == Image_t && proxy_color_set && (abs(pixel_out.r - proxy_color.r) + abs(pixel_out.g - proxy_color.g) + abs(pixel_out.b - proxy_color.b) < 7) ) {
        // a workaround was found for the converter page such that the data does not get farbled.
        if ( _ltype == Image_t && proxy_color_set && pixel_out == proxy_color ) {
            pixel_out = *rgb;
            pixel_out.a = leds[ti].alpha;
        }

        // because setBrightness() will effect the brightness of every led in every layer nscale8x3 is used instead.
        // setBrightness() should only be used in the main code
        nscale8x3(pixel_out.r, pixel_out.g, pixel_out.b, layer_brightness);

        // as layer_brightness level gets dimmer lower the alpha/increase the transparency
        uint8_t alpha_scaling_factor = 255;
        if (layer_brightness < 64) {
            alpha_scaling_factor = 4*layer_brightness;
        }
        pixel_out.a = scale8(pixel_out.a, alpha_scaling_factor);
    }

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
        case DYNAMIC_RAINBOW:
            //accelerate_decelerate_pattern(30, 2, 1000, &ReAnimator::dynamic_rainbow, dfp);
            dynamic_rainbow(50, dfp);
            break;
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
            //then it takes MTX_NUM_LEDS*draw_interval for one star sequence to be completely drawn in milliseconds
            //draw_time = MTX_NUM_LEDS*draw_interval [ms]
            //if we want to draw N stars per minute [spm] then N * draw_time must be less than 60000 [ms]
            // if MTX_NUM_LEDS is higher that means the draw_time is longer and so the spm must be lower to fit all
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
        case PENDULUM:
            pendulum();
            break;
        case FUNKY:
            funky();
            break;
        case RIFFLE:
            riffle();
            break;
        case SPARKLE:
            sparkle(20, false, 32);
            break;
        case WEAVE:
            weave(60);
            break;
        case PUCK_MAN:
            puck_man(150, dfp);
            break;
        case RAIN:
            rain(100);
            break;
        case WATERFALL:
            waterfall(100);
            break;
        case NO_PATTERN:
            //fill_solid(leds, MTX_NUM_LEDS, CRGBA::Transparent);
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
        case BREATHING:
            breathing(10);
            break;
        case FLICKER:
            flicker(150);
            break;
        case FROZEN_DECAY:
            freezer.timer(7000);
            if (freezer.is_frozen()) {
                vanish_randomly(7, 130);
                image_clean = false;
            }
            break;
    }

    return retval;
}


void ReAnimator::refresh_text(uint16_t draw_interval) {
    if (is_wait_over(draw_interval)) {
        uint32_t c = ftext.s[refresh_text_index];
        uint32_t nc = ftext.s[(refresh_text_index+1) % ftext.s.length()];
        if(shift_char(c, nc)) {
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

void ReAnimator::dynamic_rainbow(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    if (is_wait_over(draw_interval)) {
        for(uint16_t i = MTX_NUM_LEDS-1; i > 0; i--) {
            leds[(this->*dfp)(i)] = leds[(this->*dfp)(i-1)];
        }

        leds[(this->*dfp)(0)] = CHSV(((MTX_NUM_LEDS-1-dr_delta)*255/MTX_NUM_LEDS), 255, 255);

        dr_delta = (dr_delta + 1) % MTX_NUM_LEDS;
    }
}


void ReAnimator::orbit(uint16_t draw_interval, int8_t delta) {
    if (pattern != last_pattern) {
        o_pos = MTX_NUM_LEDS;
    }

    if (is_wait_over(draw_interval)) {
        uint16_t fuzz = 1 + random16(3);
        while (fuzz--) {
            fadeToTransparentBy(leds, MTX_NUM_LEDS, 20);
            if (delta > 0) {
                o_pos = o_pos % MTX_NUM_LEDS;
            }
            else {
                // o_pos underflows after it goes below zero
                if (o_pos > MTX_NUM_LEDS-1) {
                    o_pos = MTX_NUM_LEDS-1;
                }
            }

            leds[o_pos] = *rgb;
            o_pos = o_pos + delta;
        }
    }
}


void ReAnimator::general_chase(uint16_t draw_interval, uint16_t genparam, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    //genparam represents the number of LEDs involved in a pattern
    if (pattern != last_pattern) {
        gc_delta = 0;
    }

    if (is_wait_over(draw_interval)) {
        fadeToTransparentBy(leds, MTX_NUM_LEDS, (255-(genparam*8)));

        // i=i+genparam
        //genparam = 3 is confusing
        //2 gives a checkerboard
        //4 is OK
        //16, 128 are cool
        for (uint16_t i = 0; i+gc_delta < MTX_NUM_LEDS; i=i+genparam) {
            leds[(this->*dfp)(i+gc_delta)] = *rgb;
        }

        gc_delta = (gc_delta + 1) % genparam;
    }
}


void ReAnimator::running_lights(uint16_t draw_interval, uint16_t genparam, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    //genparam represents the number of waves
    if (pattern != last_pattern) {
        rl_delta = 0;
    }

    if (is_wait_over(draw_interval)) {
        uint16_t fuzz = 1 + random16(3);
        while (fuzz--) {
            for (uint16_t i = 0; i < MTX_NUM_LEDS; i++) {
                uint16_t a = genparam*(i+rl_delta)*255/(MTX_NUM_LEDS-1);
                // this pattern normally runs from right-to-left, so flip it by using negative indexing
                uint16_t ni = (MTX_NUM_LEDS-1) - i;
                CRGBA light = *rgb;
                //nscale8x3(light.r, light.g, light.b, 255-sin8(a));
                light.fadeToTransparentBy(255-sin8(a));
                leds[(this->*dfp)(ni)] = light;
            }

            rl_delta = (rl_delta + 1) % (MTX_NUM_LEDS/genparam);
        }
    }
}


//star_size – the number of LEDs that represent the star, not counting the tail of the star.
//star_trail_decay - how fast the star trail decays. A larger number makes the tail short and/or disappear faster.
//spm - stars per minute
void ReAnimator::shooting_star(uint16_t draw_interval, uint8_t star_size, uint8_t star_trail_decay, uint8_t spm, uint16_t(ReAnimator::*dfp)(uint16_t)) {  
    const uint16_t cool_down_interval = (60000-(spm*MTX_NUM_LEDS*draw_interval))/spm; // adds a delay between creation of new shooting stars

    if (pattern != last_pattern) {
        ss_start_pos = random16(0, MTX_NUM_LEDS/4);
        ss_stop_pos = random16(star_size+(MTX_NUM_LEDS/2), MTX_NUM_LEDS);
        ss_pos = ss_start_pos;
        //cdi_pm doesn't need to be reset here because it is a cool down timer
    }

    if (is_wait_over(draw_interval)) {
        fade_randomly(128, star_trail_decay);

        if ( (millis() - ss_cdi_pm) > cool_down_interval ) {
            for (uint8_t i = 0; i < star_size; i++) {
                //leds[(this->*dfp)(pos+(star_size-1)-i)] += CHSV(*hue, 255, 255);
                leds[(this->*dfp)(ss_pos+(star_size-1)-i)] += *rgb;
                // we have to subtract 1 from star_size because one piece goes at pos
                // example, if star_size = 3: [*]  [*]  [*]
                //                            pos pos+1 pos+2
            }
            ss_pos++;
            if (ss_pos+(star_size-1) >= ss_stop_pos+1) {
                ss_start_pos = random16(0, MTX_NUM_LEDS/4);
                ss_stop_pos = random16(star_size+(MTX_NUM_LEDS/2), MTX_NUM_LEDS);
                ss_pos = ss_start_pos;
                ss_cdi_pm = millis();
            }
        }
    }
}


void ReAnimator::cylon(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    if (pattern != last_pattern) {
        c_pos = 0;
        c_delta = 1;
    }

    if (is_wait_over(draw_interval)) {
        uint16_t fuzz = 1 + random16(3);
        while (fuzz--) {
            fadeToTransparentBy(leds, MTX_NUM_LEDS, 20);

            leds[(this->*dfp)(c_pos)] += *rgb;

            c_pos = c_pos + c_delta;
            if (c_pos == 0 || c_pos == MTX_NUM_LEDS-1) {
                c_delta = -c_delta;
            }
        }
    }
}


void ReAnimator::solid(uint16_t draw_interval) {
    if (is_wait_over(draw_interval)) {
        fill_solid(leds, MTX_NUM_LEDS, *rgb);
    }
}


// inspired by juggle from FastLED/examples/DemoReel00.ino -Mark Kriegsman, December 2014
void ReAnimator::pendulum() {
    const uint8_t bpm_offset = 56;
    const uint8_t num_columns = MTX_NUM_COLS;
    fadeToTransparentBy(leds, MTX_NUM_LEDS, 15);
    byte ball_hue = hue;
    const uint8_t num_balls = MTX_NUM_COLS;
    for (uint8_t i = 0; i < num_columns; i++) {
        Point p;
        p.x = i*(MTX_NUM_COLS/num_columns);
        // normally beatsin16() is based on millis(), but since this effect is called irregularly
        // in time, use a different timebase that does increase with calls to this effect in a regular way.
        // beatsin16() effectively uses this: uint16_t beat = ((millis() - timebase) * beats_per_minute_88 * 280) >> 16;
        // so to cancel out the effect of millis() use this: timebase = millis()-t
        // redefining the timebase prevents the effect from skipping
        p.y = beatsin16(i+7, 0, MTX_NUM_COLS-1, millis()-p_t);
        uint16_t j = cart2serp(p);
        leds[j] |= CHSV(ball_hue, 200, 255);
        ball_hue += 255/num_columns;
    }
    p_t+=12;
}


void ReAnimator::funky() {
    const uint8_t bpm_offset = 14;
    const uint8_t num_columns = MTX_NUM_COLS;
    byte ball_hue = hue;
    //fadeToTransparentBy(leds, MTX_NUM_LEDS, 1); // colors are washed out if this is used for this pattern
    fadeToBlackBy(leds, MTX_NUM_LEDS, 1);
    for (uint8_t i = 0; i < num_columns; i++) {
        Point p;
        p.x = i*(MTX_NUM_COLS/num_columns);
        p.y = beatsin16(i+bpm_offset, 0, MTX_NUM_COLS-1, millis()-f_t);
        uint16_t j = cart2serp(p);
        p.y = MTX_NUM_COLS-1 - beatsin16(i+bpm_offset, 0, MTX_NUM_COLS-1, millis()-f_t);
        uint16_t k = cart2serp(p);
        leds[j] |= CHSV(ball_hue, 200, 255);
        leds[k] |= CHSV(255-ball_hue, 200, 255);
        ball_hue += 255/num_columns;
    }
    f_t+=12;
}


void ReAnimator::riffle() {
    uint8_t ball_hue = hue;
    uint8_t i = 0;
    //fadeToTransparentBy(leds, MTX_NUM_LEDS, 5); // takes longer for colors to separate and appear distinct if this is used for this pattern
    fadeToBlackBy(leds, MTX_NUM_LEDS, 5);
    while (true) {
        uint16_t high = ((i+1)*16)-1;
        if (high >= MTX_NUM_LEDS/2) {
            break;
        }
        uint16_t p = beatsin16(3, 0, high, millis()-r_t);
        leds[MTX_NUM_LEDS-1-p] |= CHSV(ball_hue, 200, 255);

        uint16_t q = (MTX_NUM_COLS*(p/MTX_NUM_COLS)+MTX_NUM_COLS)-1 - p%MTX_NUM_COLS;
        leds[q] |= CHSV(ball_hue, 200, 255);

        i++;
        ball_hue += 32;
    }
    r_t += 12;
}


void ReAnimator::sparkle(uint16_t draw_interval, bool random_color, uint8_t fade) {
    CRGB c = (random_color) ? CHSV(random8(), 255, 255) : *rgb;

    if (is_wait_over(draw_interval)) {
        fadeToTransparentBy(leds, MTX_NUM_LEDS, fade);
        leds[random16(MTX_NUM_LEDS)] = c;
    }
}


void ReAnimator::weave(uint16_t draw_interval) {
    if (pattern != last_pattern) {
        w_pos = 0;
    }

    if (is_wait_over(draw_interval)) {
        fadeToTransparentBy(leds, MTX_NUM_LEDS, 20);
        leds[w_pos] += *rgb;
        leds[MTX_NUM_LEDS-1-w_pos] += (CRGBA::White - *rgb);
        w_pos = (w_pos + 2) % MTX_NUM_LEDS;
    }
}


void ReAnimator::puck_man(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t)) {
    if (pattern != last_pattern) {
        pm_puck_man_pos = 0;
    }

    if (is_wait_over(draw_interval)) {
        clear();

        if (pm_puck_man_pos == 0) {
            (*_cb)(0);
            pm_blinky_pos = (-2 + MTX_NUM_LEDS) % MTX_NUM_LEDS;
            pm_pinky_pos  = (-3 + MTX_NUM_LEDS) % MTX_NUM_LEDS;
            pm_inky_pos   = (-4 + MTX_NUM_LEDS) % MTX_NUM_LEDS;
            pm_clyde_pos  = (-5 + MTX_NUM_LEDS) % MTX_NUM_LEDS;
            pm_blinky_visible = 1;
            pm_pinky_visible = 1;
            pm_inky_visible = 1;
            pm_clyde_visible = 1;
            pm_puck_man_delta = 1;
            pm_ghost_delta = 1;
            pm_speed_jump_cnt = 0;

            // the power pellet must be at least at led[48] so the pattern completes correctly
            // 48 is close to the beginning though and that leaves a lot of boring animation of
            // just puck-man eating puck dots, so it is better to put the power pellet closer to
            // the end.
            //pm_power_pellet_pos = 48;
            // the power pellet must be at an even led so multiply by 2.
            // power pellet falls between 60% and 80% of the length of LEDs
            pm_power_pellet_pos = 2*random16((3*MTX_NUM_LEDS)/10, (4*MTX_NUM_LEDS)/10 + 1);

            for (uint16_t i = 0; i < MTX_NUM_LEDS; i+=2) {
                pm_puck_dots[i] = 1;
            }
            pm_puck_dots[pm_power_pellet_pos] = 2;
        }

        for (uint16_t i = 0; i < MTX_NUM_LEDS; i+=2) {
            leds[(this->*dfp)(i)] = (pm_puck_dots[i] == 1) ? CRGBA::White : CRGBA::Transparent;
        }

        if (pm_puck_dots[pm_power_pellet_pos] == 2) {
            if (pm_power_pellet_flash_state) {
                pm_power_pellet_flash_state = !pm_power_pellet_flash_state;
                leds[(this->*dfp)(pm_power_pellet_pos)] = CHSV(HUE_RED, 255, 255);
            }
            else {
                pm_power_pellet_flash_state = !pm_power_pellet_flash_state;
                leds[(this->*dfp)(pm_power_pellet_pos)] = CRGBA::Transparent;
            }
        }

        if (pm_puck_dots[pm_power_pellet_pos] == 2) {
            leds[(this->*dfp)(pm_blinky_pos)] = CHSV(HUE_RED, 255, pm_blinky_visible*255);
            leds[(this->*dfp)(pm_pinky_pos)]  = CHSV(HUE_PINK, 255, pm_pinky_visible*255);
            leds[(this->*dfp)(pm_inky_pos)]   = CHSV(HUE_AQUA, 255, pm_inky_visible*255);
            leds[(this->*dfp)(pm_clyde_pos)]  = CHSV(HUE_ORANGE, 255, pm_clyde_visible*255);
        }
        else if (pm_blinky_visible || pm_pinky_visible || pm_inky_visible || pm_clyde_visible) {
            (*_cb)(1);
            pm_puck_man_delta = -3;
            pm_ghost_delta = -2;

            pm_ghost_delta = -2;
            if (pm_speed_jump_cnt < 5)
              pm_puck_man_delta = -1;
            else if (pm_speed_jump_cnt < 10) {
              pm_puck_man_delta = -2;
            }
            else {
              pm_puck_man_delta = -3;
            }
            pm_speed_jump_cnt++;

            if (pm_puck_man_pos == pm_blinky_pos) {
                (*_cb)(2);
                pm_blinky_visible = 0;
            }
            else if (pm_puck_man_pos == pm_pinky_pos) {
                pm_pinky_visible = 0;
            }
            else if (pm_puck_man_pos == pm_inky_pos) {
                pm_inky_visible = 0;
            }
            else if (pm_puck_man_pos == pm_clyde_pos) {
                pm_clyde_visible = 0;
            }

            leds[(this->*dfp)(pm_blinky_pos)] = CHSV(HUE_BLUE, 255, pm_blinky_visible*255);
            leds[(this->*dfp)(pm_pinky_pos)]  = CHSV(HUE_BLUE, 255, pm_pinky_visible*255);
            leds[(this->*dfp)(pm_inky_pos)]   = CHSV(HUE_BLUE, 255, pm_inky_visible*255);
            leds[(this->*dfp)(pm_clyde_pos)]  = CHSV(HUE_BLUE, 255, pm_clyde_visible*255);

        }
        else {
            pm_puck_man_delta = 1;
        }

        pm_blinky_pos = pm_blinky_pos + pm_ghost_delta;
        pm_blinky_pos = (MTX_NUM_LEDS+pm_blinky_pos) % MTX_NUM_LEDS;
        pm_pinky_pos = pm_pinky_pos + pm_ghost_delta;
        pm_pinky_pos = (MTX_NUM_LEDS+pm_pinky_pos) % MTX_NUM_LEDS;
        pm_inky_pos = pm_inky_pos + pm_ghost_delta;
        pm_inky_pos = (MTX_NUM_LEDS+pm_inky_pos) % MTX_NUM_LEDS;
        pm_clyde_pos = pm_clyde_pos + pm_ghost_delta;
        pm_clyde_pos = (MTX_NUM_LEDS+pm_clyde_pos) % MTX_NUM_LEDS;

        leds[(this->*dfp)(pm_puck_man_pos)] = CHSV(HUE_YELLOW, 255, 255);
        pm_puck_dots[pm_puck_man_pos] = 0;

        pm_puck_man_pos = pm_puck_man_pos + pm_puck_man_delta;
        pm_puck_man_pos = (MTX_NUM_LEDS+pm_puck_man_pos) % MTX_NUM_LEDS;
    }

}


// good for an effect similar to the falling text in The Matrix
void ReAnimator::rain(uint16_t draw_interval) {
    if (is_wait_over(draw_interval)) {
        for (uint8_t i = 0; i < MTX_NUM_COLS; i++) {
            Point p;
            p.x = i;
            for (uint8_t j = 1; j < MTX_NUM_ROWS; j++) {
              p.y = MTX_NUM_ROWS - j;
              uint16_t idxf = cart2serp(p);
              p.y = MTX_NUM_ROWS - j - 1;
              uint16_t idxi = cart2serp(p);
              leds[idxf] = leds[idxi];
            }

            p.y = 0;
            uint16_t idx = cart2serp(p);
            if (random8() > 245) {
                leds[idx] = *rgb;
            }
            else {
                leds[idx].fadeToTransparentBy(70);
            }
        }
    }
}


void ReAnimator::waterfall(uint16_t draw_interval) {
    if (is_wait_over(draw_interval)) {
        fadeToTransparentBy(leds, MTX_NUM_LEDS, 40);
        for (uint8_t i = 0; i < MTX_NUM_COLS; i++) {
            Point p;
            p.x = i;
            for (uint8_t j = 1; j < MTX_NUM_ROWS; j++) {
              p.y = MTX_NUM_ROWS - j;
              uint16_t idxf = cart2serp(p);
              p.y = MTX_NUM_ROWS - j - 1;
              uint16_t idxi = cart2serp(p);
              leds[idxf] = leds[idxi];
              leds[idxf].a = 255;
            }

            p.y = 0;
            uint16_t idx = cart2serp(p);
            if (random8() > 175) {
                leds[idx] = *rgb;
            }
        }
    }
}




// ++++++++++++++++++++++++++++++
// ++++++++++ OVERLAYS ++++++++++
// ++++++++++++++++++++++++++++++
void ReAnimator::breathing(uint16_t interval) {
    const uint8_t min_brightness = 2;
    static uint8_t delta = 0; // goes up to 255 then overflows back to 0

    if (finished_waiting(interval)) {
        extern uint8_t homogenized_brightness;
        uint8_t max_brightness = homogenized_brightness;
        layer_brightness = scale8(triwave8(delta), max_brightness-min_brightness)+min_brightness;
        delta++;
    }
}


void ReAnimator::flicker(uint16_t interval) {
    //fade_randomly(10, 150);

    // an on or off period less than 16 ms probably can't be perceived
    if (finished_waiting(interval)) {
        // because FastLED is managing the brightness, flicker is simply on or off, since we are not transitioning up to the max it is OK to use 255 to mean show at highest brightness allowed
        // instead of using the actual max brightness allowed
        layer_brightness = (random8(1,11) > 4)*255;
    }
}


void ReAnimator::fade_randomly(uint8_t chance_of_fade, uint8_t decay) {
    for (uint16_t i = 0; i < MTX_NUM_LEDS; i++) {
        if (chance_of_fade > random8()) {
            leds[i].fadeToBlackBy(decay);
        }
    }
}


void ReAnimator::vanish_randomly(uint8_t chance_of_fade, uint8_t decay) {
    for (uint16_t i = 0; i < MTX_NUM_LEDS; i++) {
        if (chance_of_fade > random8()) {
            leds[i].fadeToTransparentBy(decay);
        }
    }
}



// ++++++++++++++++++++++++++++++
// +++++++++++ IMAGE ++++++++++++
// ++++++++++++++++++++++++++++++
bool ReAnimator::load_image_from_file(String fs_path, String* message) {
    bool retval = false;
    if (fs_path == "") {
      return false;
    }
    File file = LittleFS.open(fs_path, "r");

    if(!file){
        if (message) {
            *message = F("set_image(): File not found.");
        }
        return false;
    }
    if (file.available()) {
        DynamicJsonDocument doc(8192);
        ReadBufferingStream bufferedFile(file, 64);
        DeserializationError error = deserializeJson(doc, bufferedFile);
        if (error) {
            if (message) {
                *message = F("set_image(): deserializeJson() failed.");
            }
            return false;
        }

        JsonObject object = doc.as<JsonObject>();

        proxy_color_set = false;
        if (!object[F("pc")].isNull()) {
            std::string pc = object[F("pc")].as<std::string>();
            if (!pc.empty()) {
                proxy_color = std::stoul(pc, nullptr, 16);
                proxy_color_set = true;
            }
        }

        clear();
        retval = deserializeSegment(object, leds, MTX_NUM_LEDS);
        if (message) {
            *message = F("set_image(): deserializeSegment() had error.");
        }
    }
    file.close();

    if (message) {
        *message = F("set_image(): Matrix loaded.");
    }
    return retval;
}



// ++++++++++++++++++++++++++++++
// ++++++++++++ TEXT ++++++++++++
// ++++++++++++++++++++++++++++++
const uint8_t* ReAnimator::get_bitmap(const lv_font_t* f, uint32_t c, uint32_t nc, uint32_t* full_width, uint16_t* box_w, uint16_t* box_h, int16_t* offset_y) {
    // comments from lv_font_fmt_txt.h
    //uint32_t bitmap_index;          //< Start index of the bitmap. A font can be max 4 GB.
    //uint32_t adv_w;                 //< Draw the next glyph after this width. 28.4 format (real_value * 16 is stored).
    //uint16_t box_w;                 //< Width of the glyph's bounding box
    //uint16_t box_h;                 //< Height of the glyph's bounding box
    //int16_t ofs_x;                  //< x offset of the bounding box
    //int16_t ofs_y;                  //< y offset of the bounding box. Measured from the top of the line
    lv_font_glyph_dsc_t g;
    bool g_ret = lv_font_get_glyph_dsc(f, &g, c, nc);
    if (g_ret && g.gid.index) {
        lv_font_fmt_txt_dsc_t* fdsc = (lv_font_fmt_txt_dsc_t*)f->dsc;
        const lv_font_fmt_txt_glyph_dsc_t* gdsc = &fdsc->glyph_dsc[g.gid.index];
        const uint8_t* glyph = &fdsc->glyph_bitmap[gdsc->bitmap_index];

        if (full_width != nullptr) {
            // g.adv_w is not the same as the adv_w font in the glyph descriptor in the font file
            // lv_font_get_glyph_dsc_fmt_txt divides that number by 16 and includes kerning
            (*full_width) = g.adv_w;
        }
        if (box_w != nullptr) {
            (*box_w) = g.box_w;
        }
        if (box_h != nullptr) {
            (*box_h) = g.box_h;
        }
        if (offset_y != nullptr) {
            (*offset_y) = g.ofs_y;
        }

        return glyph;
    }
    return NULL;
}


bool ReAnimator::shift_char(uint32_t c, uint32_t nc) {
    if (shift_char_tracking) {
        for (uint8_t i = 0; i < MTX_NUM_ROWS; i++) {
            for (uint8_t j = 0; j < MTX_NUM_COLS-1; j++) {
                uint8_t k = (MTX_NUM_COLS-1)-j;
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
            leds[cart2serp(p)] = CRGBA::Transparent; // transparent black

        }
        shift_char_tracking--;
        return false;
    }

    bool finished_shifting = false;
    uint32_t full_width = 0;
    uint16_t box_w = 0;
    uint16_t box_h = 0;
    int16_t offset_y = 0;

    const uint8_t* glyph = get_bitmap(font, c, nc, &full_width, &box_w, &box_h, &offset_y);
    if (glyph) {
        uint16_t glyph_row = 0;
        for (uint8_t i = 0; i < MTX_NUM_ROWS; i++) {
            for (uint8_t j = 0; j < MTX_NUM_COLS-1; j++) {
                uint8_t k = (MTX_NUM_COLS-1)-j;
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

            // instead of dimming the pixel color to match the glyph's brightness we use transparency
            // where a transparency of 0 represents the glyph's negative space
            uint8_t alpha = 0;
            // do not start drawning the glyph until we are on the right line to ensure the glyph is in the
            // orrect position relative to the other characters: MTX_NUM_ROWS-box_h-offset_y-ftext.baseline
            // and that the text is vertically centered: ftext.vmargin
            if (i >= (MTX_NUM_ROWS-box_h-offset_y-ftext.baseline) - ftext.vmargin && glyph_row < box_h) {
                alpha = glyph[(glyph_row*box_w)+shift_char_column];
                glyph_row++;
            }
            CRGB pixel = *rgb;
            if (alpha == 0) {
                pixel = CRGB::Black;
            }
            leds[cart2serp(p)] = pixel;
            leds[cart2serp(p)].a = alpha;
        }

        // full_width should be glyph's adv_w specified in the font file divided by 16 plus kerning
        // see lv_font_get_bitmap_fmt_txt() in lv_font_minimal.c
        // however kerning does not appear to be implemented for fonts output by the online font converter
        // using just the glyph's box_w gives better results
        // whitespace (just space U+0020?) glyphs have a box_w of 0 so their full width must be used instead
        // however you will likely get better results if you manually edit the font file to set the box_w of
        // whitespace characters to be closer to the average character width.
        uint16_t shift_width = box_w ? box_w : full_width;
        shift_char_column = (shift_char_column+1)%shift_width;
        if (shift_char_column == 0) {
            finished_shifting = true; // character fully shifted onto matrix.
            shift_char_tracking = 1; // add tracking (spacing between letters) on next call
        }
    }

    return finished_shifting;
}



// ++++++++++++++++++++++++++++++
// ++++++++++++ INFO ++++++++++++
// ++++++++++++++++++++++++++++++
void ReAnimator::refresh_date_time(uint16_t draw_interval) {
    static uint8_t get_time_fails = 5; // initially shows all dashes if time is not available on the first call to getLocalTime()
    if (is_wait_over(draw_interval)) {
        const uint8_t nd = 4;
        struct tm local_now = {0};
        // the second argument of getLocalTime indicates how long it should loop waiting for the time to be received from ntp
        // since getLocalTime() is already being called around every 200 ms (via the if above) there is no need for getLocalTime()
        // to loop its code, so set the second argument to 0. using 0 also keeps the other animations from being delayed by 
        // getLocalTime() looping.
        // calling it like this does not spam the ntp server.
        char ts[nd+1] = {'-', '-', '-', '-', '\0'};
        if (getLocalTime(&local_now, 0)) {
            get_time_fails = 0;
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
        }
        else {
            // after 5 fails we want to show all dashes to indicate getting the time is failing
            if (get_time_fails < 5) {
                return;
            }
            get_time_fails++;
            get_time_fails = (get_time_fails > 5) ? 5 : get_time_fails;
        }

        extern lv_font_t seven_segment;
        const lv_font_t* clkfont = &seven_segment;
        uint16_t max_width = 0;
        uint16_t max_height = 0;
        get_bitmap(clkfont, '0', '\0', nullptr, &max_width, &max_height);
        const uint8_t left_col = (MTX_NUM_COLS/2)+max_width;
        const uint8_t right_col = (MTX_NUM_COLS/2)-2;
        const uint8_t top_row = 0;
        const uint8_t bottom_row = (MTX_NUM_ROWS/2)+1;
        const Point corners[nd] = {{.x = left_col, .y = top_row}, {.x = right_col, .y = top_row}, {.x = left_col, .y = bottom_row}, {.x = right_col, .y = bottom_row}};

        for (uint8_t ci = 0; ci < nd; ci++) {
            char c = ts[ci];
            uint16_t box_w = 0;
            uint16_t box_h = 0;
            int16_t offset_y = 0;
            const uint8_t* glyph = get_bitmap(clkfont, c, '\0', nullptr, &box_w, &box_h, &offset_y);

            if (glyph) {
                uint8_t n = 0;
                Point p0 = corners[ci];
                for (uint8_t i = 0; i < max_height; i++) {
                    // not all characters are max_width so we cannot count on their negative space overwriting the previous character
                    // therefore we have to loop across max_width so the previous character can be overwritten with alpha = 0
                    // even if the bitmap is not specified for that area.
                    for (uint8_t j = 0; j < max_width; j++) {
                        Point p;
                        p.x = p0.x-j;
                        p.y = p0.y+i;

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

                        uint8_t alpha = 0;
                        if (offset_y <= i && i < box_h + offset_y) {
                            // j >= max_width - box_w aligns characters to the right 
                            if (j >= max_width - box_w) {
                                //alpha = (glyph[n] == 0) ? 0 : 255; // remove partial transparency
                                alpha = glyph[n];
                                n++;
                            }
                        }
                        CRGB pixel = *rgb;
                        if (alpha == 0) {
                            pixel = CRGB::Black;
                        }
                        leds[cart2serp(p)] = pixel;
                        leds[cart2serp(p)].a = alpha;
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


void ReAnimator::flip(CRGB* sm, bool dim) {
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
        dx = (xi >= MTX_NUM_COLS) ? -abs(xi) : xi;
        dy = (yi >= MTX_NUM_COLS) ? -abs(yi) : yi;
    }

    uint16_t ti = MTX_NUM_LEDS; // used to indicate pixel is not in bounds and should not be drawn.

    Point p1 = serp2cart(i);
    Point p2;

    if (t_has_entered && !wrap && !t_visible) {
        // wrapping is not turned on and input has passed through matrix never to return
        return ti;
    }

    t_visible = false;
    gap = (gap == -1) ? MTX_NUM_COLS : gap; // if gap is -1 set gap to MTX_NUM_COLS so that the input only appears in one place but will still loop around

    uint8_t ux = (sx > 0) ? MTX_NUM_COLS-1-p1.x: p1.x; // flip heading output travels
    uint8_t uy = (sy > 0) ? MTX_NUM_COLS-1-p1.y: p1.y;

    int8_t vx = ux+dx; // shift input over into output by dx
    int8_t vy = uy+dy;

    if (t_has_entered && wrap) {
        vx = vx % (MTX_NUM_COLS+gap);
        vy = vy % (MTX_NUM_COLS+gap);
    }

    if ( 0 <= vx && vx < MTX_NUM_COLS && 0 <= vy && vy < MTX_NUM_COLS ) {
        t_visible = true;
        vx = (sx > 0) ? MTX_NUM_COLS-1-vx : vx; // flip image
        vy = (sy > 0) ? MTX_NUM_COLS-1-vy : vy;
        p2.x = vx;
        p2.y = vy;
        ti = cart2serp(p2);
    }

    // not sure if I like stopping movement when frozen.
    //if (i == MTX_NUM_LEDS-1 && !freezer.is_frozen()) {
    if (i == MTX_NUM_LEDS-1) {
        dx += abs(sx%MTX_NUM_COLS);
        dy += abs(sy%MTX_NUM_COLS);
        // need to track when input has entered into view for the first time
        // to prevent wrapping until the transient period has ended
        // this allows controlling how long it takes the input to enter the matrix
        // by setting abs(xi) or abs(yi) to higher numbers.
        if (dx >= 0 && dy >= 0) {
            t_has_entered = true;
        }

        if (t_has_entered && wrap) {
                dx = dx % (MTX_NUM_COLS+gap);
                dy = dy % (MTX_NUM_COLS+gap);
        }
    }
    return ti;
}


//void ReAnimator::ntranslate(CRGBA in[MTX_NUM_LEDS], CRGBA out[MTX_NUM_LEDS], int8_t xi, int8_t yi, int8_t sx, int8_t sy, bool wrap, int8_t gap) {
void ReAnimator::ntranslate(CRGBA in[], CRGBA out[], int8_t xi, int8_t yi, int8_t sx, int8_t sy, bool wrap, int8_t gap) {
// an output array is required to use this function. out[] should be what is displayed on the matrix.
    if (t_initial) {
        t_initial = false;
        t_visible = false;
        t_has_entered = false;
        dx = (xi >= MTX_NUM_COLS) ? -abs(xi) : xi;
        dy = (yi >= MTX_NUM_COLS) ? -abs(yi) : yi;
    }

    Point p1;
    Point p2;

    if (t_has_entered && !wrap && !t_visible) {
        // wrapping is not turned on and input has passed through matrix never to return
        return;
    }

    t_visible = false;
    gap = (gap == -1) ? MTX_NUM_COLS : gap; // if gap is -1 set gap to MTX_NUM_COLS so that the input only appears in one place but will still loop around

    for (uint8_t j = 0; j < MTX_NUM_COLS; j++) {
        for (uint8_t k = 0; k < MTX_NUM_COLS; k++) {
            uint8_t ux = (sx > 0) ? MTX_NUM_COLS-1-k: k; // flip heading output travels
            uint8_t uy = (sy > 0) ? MTX_NUM_COLS-1-j: j;

            p1.x = ux;
            p1.y = uy;
            out[cart2serp(p1)] = CRGBA::Transparent;

            int8_t vx = k+dx; // shift input over into output by d
            int8_t vy = j+dy;
            if (t_has_entered && wrap) {
                vx = vx % (MTX_NUM_COLS+gap);
                vy = vy % (MTX_NUM_COLS+gap);
            }

            if ( 0 <= vx && vx < MTX_NUM_COLS && 0 <= vy && vy < MTX_NUM_COLS ) {
                t_visible = true;
                vx = (sx > 0) ? MTX_NUM_COLS-1-vx : vx; // flip image
                vy = (sy > 0) ? MTX_NUM_COLS-1-vy : vy;
                p2.x = vx;
                p2.y = vy;
                out[cart2serp(p1)] = in[cart2serp(p2)];
            }
        }
    }

    dx += abs(sx%MTX_NUM_COLS);
    dy += abs(sy%MTX_NUM_COLS);
    // need to track when input has entered into view for the first time
    // to prevent wrapping until the transient period has ended
    // this allows controlling how long it takes the input to enter the matrix
    // by setting abs(xi) or abs(yi) to higher numbers.
    if (dx >= 0 && dy >= 0) {
        t_has_entered = true;
    }

    if (t_has_entered && wrap) {
        dx = dx % (MTX_NUM_COLS+gap);
        dy = dy % (MTX_NUM_COLS+gap);
    }
}


uint16_t ReAnimator::mover(uint16_t i) {
    uint16_t ti;
    // to be replaced by speed option in the future.
    int8_t s = 1;
    // add a bit of variability to the speed so moving objects do not always overlap in the same spot
    if (random8(1, 11) > 8) {
        s = 2;
    }
    switch (heading) {
        default:
        case 0:
            ti = i;
            break;
        case 1:
            ti = translate(i, 0, MTX_NUM_COLS, 0, -s, true, -1);
            break;
        case 2:
            ti = translate(i, MTX_NUM_COLS, MTX_NUM_COLS, -s, -s, true, -1);
            break;
        case 3:
            ti = translate(i, MTX_NUM_COLS, 0, -s, 0, true, -1);
            break;
        case 4:
            ti = translate(i, MTX_NUM_COLS, -MTX_NUM_COLS, -s, s, true, -1);
            break;
        case 5:
            ti = translate(i, 0, -MTX_NUM_COLS, 0, s, true, -1);
            break;
        case 6:
            ti = translate(i, -MTX_NUM_COLS, -MTX_NUM_COLS, s, s, true, -1);
            break;
        case 7:
            ti = translate(i, -MTX_NUM_COLS, 0, s, 0, true, -1);
            break;
        case 8:
            ti = translate(i, -MTX_NUM_COLS, MTX_NUM_COLS, s, -s, true, -1);
            break;
    }
    return ti;
}


// ++++++++++++++++++++++++++++++
// ++++++++++ HELPERS +++++++++++
// ++++++++++++++++++++++++++++++
void ReAnimator::fadeToBlackBy(CRGBA leds[], uint16_t num_leds, uint8_t fadeBy) {
    for (uint16_t i = 0; i < num_leds; i++) {
        leds[i].fadeToBlackBy(fadeBy);
    }
}

void ReAnimator::fadeToTransparentBy(CRGBA leds[], uint16_t num_leds, uint8_t fadeBy) {
    for (uint16_t i = 0; i < num_leds; i++) {
        leds[i].fadeToTransparentBy(fadeBy);
    }
}

void ReAnimator::fill_solid(CRGBA leds[], uint16_t num_leds, const CRGB& color) {
    for(uint16_t i = 0; i < num_leds; ++i) {
        leds[i] = color;
    }
}


uint16_t ReAnimator::forwards(uint16_t index) {
    return index;
}


uint16_t ReAnimator::backwards(uint16_t index) {
    return (MTX_NUM_LEDS-1)-index;
}


// If two functions running close to each other both call is_wait_over()
// the one with the shorter interval will reset the timer such that the
// function with the longer interval will never see its interval has
// elapsed, therefore a second function that does the same thing as
// is_wait_over() has been added. This is only a concern when a pattern
// function and an overlay function are both called at the same time.
// Patterns should use is_wait_over() and overlays should use finished_waiting(). 
bool ReAnimator::is_wait_over(uint16_t interval) {
    if ( (millis() - iwopm) > interval ) {
        iwopm = millis();
        return true;
    }
    else {
        return false;
    }
}


bool ReAnimator::finished_waiting(uint16_t interval) {
    if ( (millis() - fwpm) > interval ) {
        fwpm = millis();
        return true;
    }
    else {
        return false;
    }
}


void ReAnimator::accelerate_decelerate_pattern(uint16_t draw_interval_initial, uint16_t delta_initial, uint16_t update_period, uint16_t genparam, void(ReAnimator::*pfp)(uint16_t, uint16_t, uint16_t(ReAnimator::*dfp)(uint16_t)), uint16_t(ReAnimator::*dfp)(uint16_t)) {
    if (pattern != last_pattern) {
        adp_draw_interval = draw_interval_initial;
        adp_delta = delta_initial;
    }

    if (finished_waiting(update_period)) {
        adp_draw_interval = adp_draw_interval - adp_delta;
        if (adp_draw_interval <= 0 || adp_draw_interval >= draw_interval_initial) {
            adp_delta = -1*adp_delta;
        }
    }

    (this->*pfp)(adp_draw_interval, genparam, dfp);
}


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
            if (pos+i < MTX_NUM_LEDS) {
                leds[(this->*dfp)(pos+i)] += leds[(this->*dfp)(pos)];
                leds[(this->*dfp)(pos+i)].fadeToBlackBy(120+(i*120/abs(blur_num)));
            }
        }
    }
}


void ReAnimator::fission() {
    for (uint16_t i = MTX_NUM_LEDS-1; i > MTX_NUM_LEDS/2; i--) {
        leds[i] = leds[i-1];
    }

    for (uint16_t i = 0; i < MTX_NUM_LEDS/2; i++) {
        leds[i] = leds[i+1];
    }
}


// freeze_interval must be greater than m_failsafe_timeout
void ReAnimator::Freezer::timer(uint16_t freeze_interval) {
    if ((millis() - m_pm) > freeze_interval) {
        m_pm = millis();
        m_frozen_previous_millis = m_pm;
        m_frozen = true;
    }
}


bool ReAnimator::Freezer::is_frozen() {
    if ((millis() - m_frozen_previous_millis) > m_frozen_duration) {
        m_frozen = false;
        m_all_black = false;
        m_frozen_duration = m_failsafe_timeout;
    }
    else if (!m_all_black) {
        for (uint16_t i = 0; i < parent.MTX_NUM_LEDS; i++) {
            m_all_black = true;
            if (parent.leds[i] != CRGBA::Transparent) {
                m_all_black = false;
                break;
            }
        }
        if (m_all_black && ((m_frozen_previous_millis + m_failsafe_timeout) - millis()) > m_after_all_black_pause) {
            // after all the LEDs are found to be dark unfreeze after a short pause
            m_frozen_previous_millis = millis();
            m_frozen_duration = m_after_all_black_pause;
        }
    }

    return m_frozen;
}

/*
void ReAnimator::print_dt() {
    static uint32_t pm = 0; // previous millis
    DEBUG_PRINT("dt: ");
    DEBUG_PRINTLN(millis() - pm);
    pm = millis();
}
*/
