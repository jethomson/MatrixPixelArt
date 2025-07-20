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

#pragma once

#include "project.h"
#include "lvgl_fonts/lvgl/lvgl.h"
#include <queue>


enum LayerType {Pattern_t = 0, Accent_t = 1, Image_t = 2, Text_t = 3, Info_t = 4};

enum Pattern {
              DYNAMIC_RAINBOW = 0, SOLID = 1,
              ORBIT = 2, RUNNING_LIGHTS = 3,
              RIFFLE = 4, SPARKLE = 5, 
              WEAVE = 6, PENDULUM = 7, BINARY_SYSTEM = 8, 
              SHOOTING_STAR = 9, PUCK_MAN = 10, CYLON = 11,
              FUNKY = 12, RAIN = 13, WATERFALL = 14,
              NO_PATTERN = 50,
              THEATER_CHASE = 51,
              CHECKERBOARD = 60 
             };

enum Overlay {NO_OVERLAY = 0, BREATHING = 1, FLICKER = 2, FROZEN_DECAY = 3};

enum Info {TIME_12HR = 0, TIME_24HR = 1, DATE_MMDD = 2, DATE_DDMM = 3, TIME_12HR_DATE_MMDD = 4, TIME_24HR_DATE_DDMM = 5};


class ReAnimator {
    uint8_t MTX_NUM_COLS = 0;
    uint8_t MTX_NUM_ROWS = 0;
    uint16_t MTX_NUM_LEDS = 0;

    CRGBA* leds;

    //LayerType _ltype;
    int8_t _id ;

    uint8_t layer_brightness;

    bool autocycle_enabled;
    uint32_t autocycle_previous_millis;
    uint32_t autocycle_interval;

    bool flipflop_enabled;
    uint32_t flipflop_previous_millis;
    uint32_t flipflop_interval;

    Pattern pattern;
    Pattern last_pattern;
    Overlay transient_overlay;
    Overlay persistent_overlay;

    // direction indicates why point the leading pixel of a pattern advances toward: end of strip or beginning of strip.
    uint16_t(ReAnimator::*direction_fp)(uint16_t);
    uint16_t(ReAnimator::*antidirection_fp)(uint16_t);
    bool reverse;

    // the frontend color picker is focused on RGB so try to honor the RGB color picked.
    // sometimes it makes more sense to work with a hue, so we have a hue variable that is derived from rgb.
    // color can be determined externally (*rgb) or internally (internal_rgb) to the layer.
    // if internal_rgb is user rgb points to that.
    CRGB internal_rgb;
    CRGB* rgb;
    uint8_t hue;

    void(*_cb)(uint8_t);

    class Freezer {
        ReAnimator &parent;
        const uint16_t m_after_all_black_pause = 500;
        const uint16_t m_failsafe_timeout = 6000; // max time to stay frozen
        bool m_all_black = false; // if all leds are black before m_frozen_duration elapses then unfreeze
        uint16_t m_frozen_duration = m_failsafe_timeout;
        bool m_frozen = false;
        uint32_t m_frozen_previous_millis = 0;
        uint32_t m_pm = 0;

      public:
        Freezer(ReAnimator &r);
        void timer(uint16_t freeze_interval); // controls how often a freeze happens
        bool is_frozen();
    };

    Freezer freezer;

    // puck-man variables
    uint16_t pm_puck_man_pos;
    int8_t pm_puck_man_delta;
    uint16_t pm_blinky_pos;
    uint16_t pm_pinky_pos;
    uint16_t pm_inky_pos;
    uint16_t pm_clyde_pos;
    uint8_t pm_blinky_visible;
    uint8_t pm_pinky_visible;
    uint8_t pm_inky_visible;
    uint8_t pm_clyde_visible;
    int8_t pm_ghost_delta;
    uint16_t pm_power_pellet_pos;
    bool pm_power_pellet_flash_state;
    uint8_t* pm_puck_dots;
    uint8_t pm_speed_jump_cnt;

    uint16_t dr_delta;
    uint16_t o_pos;
    uint16_t gc_delta;
    uint16_t rl_delta;
    uint16_t ss_start_pos;
    uint16_t ss_stop_pos;
    uint32_t ss_cdi_pm; // cool_down_interval_previous_millis
    uint16_t ss_pos;
    uint16_t c_pos;
    int8_t c_delta;
    uint32_t p_t;
    uint32_t f_t;
    uint32_t r_t;
    uint16_t w_pos;
    uint16_t adp_draw_interval;
    int8_t adp_delta;

    String image_path;
    CRGB proxy_color;
    bool proxy_color_set;
    bool image_dequeued;
    bool image_loaded; // tracks whether image loading into leds[] has completed. helps prevent flicker by trying to display leds[] that is blank or has a partial image.
    bool image_clean; // tracks whether frozen decay (or possibly other accents) have corrupted the image indicating it needs a refresh
    uint32_t image_queued_time;

    typedef struct Image {
      String* image_path;
      uint16_t* MTX_NUM_LEDS;
      CRGBA* leds;
      bool* proxy_color_set;
      CRGB* proxy_color;
      bool* image_dequeued;
      bool* image_loaded;
      bool* image_clean;
    } Image;

    static QueueHandle_t qimages;

    struct Point {
      uint8_t x;
      uint8_t y;
    };

    const lv_font_t* font;

    struct fstring {
      std::string s;
      int16_t line_height = 0;
      int16_t baseline = 0;
      int16_t vmargin = 0; // for centering text vertically
      uint8_t tracking = 1; // spacing between letters
    } ftext;


    uint16_t refresh_text_pos;
    uint8_t shift_char_column;
    uint8_t shift_char_tracking; // spacing between letters

    // heading indicates which direction an object (image, patter, text, etc.) displayed on the matrix moves
    uint8_t heading;
    bool t_initial;
    bool t_visible;
    bool t_has_entered;
    uint16_t t_pixel_count;
    int8_t dx;
    int8_t dy;

    uint32_t iwopm; // previous millis for is_wait_over()
    uint32_t fwpm ; // previous millis for finished_waiting()

  public:
    LayerType _ltype;
    uint32_t display_duration; // amount of time image is shown for if it is part of an animation

    ReAnimator(uint8_t num_rows, uint8_t num_cols);
    ~ReAnimator() { delete[] leds; leds = nullptr; delete[] pm_puck_dots; pm_puck_dots = nullptr;}

    void setup(LayerType ltype, int8_t id);

    uint32_t get_autocycle_interval();
    void set_autocycle_interval(uint32_t inteval);
    bool get_autocycle_enabled();
    void set_autocycle_enabled(bool enabled);

    uint32_t get_flipflop_interval();
    void set_flipflop_interval(uint32_t inteval);
    bool get_flipflop_enabled();
    void set_flipflop_enabled(bool enabled);

    Pattern get_pattern();
    int8_t set_pattern(Pattern pattern);
    int8_t set_pattern(Pattern pattern, bool reverse);
    int8_t set_pattern(Pattern pattern, bool reverse, bool disable_autocycle_flipflop);
    int8_t increment_pattern();
    int8_t increment_pattern(bool disable_autocycle_flipflop);

    Overlay get_overlay(bool is_persistent);
    int8_t set_overlay(Overlay overlay, bool is_persistent);
    void increment_overlay(bool is_persistent);

    void set_image(String fs_path, uint32_t duration = REFRESH_INTERVAL, String* message = nullptr);
    static void load_image_from_queue(void* parameter);
    int8_t get_image_status();
    void set_text(std::string t);
    void set_info(Info id);

    void set_color(CRGB *color);
    void set_color(CRGB color);

    void set_cb(void(*cb)(uint8_t));
    void set_heading(uint8_t h);

    void clear();
    void reanimate();
    CRGBA get_pixel(uint16_t i);

  private:
    int8_t run_pattern(Pattern pattern);
    int8_t apply_overlay(Overlay overlay);
    void refresh_text(uint16_t draw_interval);
    void refresh_info(uint16_t draw_interval);


// ++++++++++++++++++++++++++++++
// ++++++++++ PATTERNS ++++++++++
// ++++++++++++++++++++++++++++++
    void dynamic_rainbow(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t));
    void orbit(uint16_t draw_interval, int8_t delta);
    void general_chase(uint16_t draw_interval, uint16_t genparam, uint16_t(ReAnimator::*dfp)(uint16_t));
    void running_lights(uint16_t draw_interval, uint16_t genparam, uint16_t(ReAnimator::*dfp)(uint16_t));
    void shooting_star(uint16_t draw_interval, uint8_t star_size, uint8_t star_trail_decay, uint8_t spm, uint16_t(ReAnimator::*dfp)(uint16_t));
    void cylon(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t));
    void solid(uint16_t draw_interval);
    void pendulum();
    void funky();
    void riffle();
    void bubbles(uint16_t draw_interval);
    void sparkle(uint16_t draw_interval, bool random_color, uint8_t fade);
    void weave(uint16_t draw_interval);
    void puck_man(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t));
    void rain(uint16_t draw_interval);
    void waterfall(uint16_t draw_interval);



// ++++++++++++++++++++++++++++++
// ++++++++++ OVERLAYS ++++++++++
// ++++++++++++++++++++++++++++++
    void breathing(uint16_t interval);
    void flicker(uint16_t interval);
    void glitter(uint16_t chance_of_glitter);
    void fade_randomly(uint8_t chance_of_fade, uint8_t decay);
    void vanish_randomly(uint8_t chance_of_fade, uint8_t decay);


// ++++++++++++++++++++++++++++++
// +++++++++++ IMAGE ++++++++++++
// ++++++++++++++++++++++++++++++
  bool load_image_from_file(String fs_path, String* message = nullptr);


// ++++++++++++++++++++++++++++++
// ++++++++++++ TEXT ++++++++++++
// ++++++++++++++++++++++++++++++
    uint16_t get_UTF8_char(const char* str, uint32_t& codepoint);
    const uint8_t* get_bitmap(const lv_font_t* f, uint32_t c, uint32_t nc = '\0', uint32_t* full_width = nullptr, uint16_t* box_w = nullptr, uint16_t* box_h = nullptr, int16_t* offset_y = nullptr);
    bool shift_char(uint32_t c, uint32_t nc = '\0');


// ++++++++++++++++++++++++++++++
// ++++++++++++ INFO ++++++++++++
// ++++++++++++++++++++++++++++++
    void refresh_date_time(uint16_t draw_interval);


// ++++++++++++++++++++++++++++++
// ++++++++++ HELPERS +++++++++++
// ++++++++++++++++++++++++++++++
    void fadeToBlackBy(CRGBA* leds, uint16_t num_leds, uint8_t fadeBy);
    void fadeToTransparentBy(CRGBA leds[], uint16_t num_leds, uint8_t fadeBy);
    void fill_solid(CRGBA* leds, uint16_t num_leds, const CRGB& color);

    uint16_t forwards(uint16_t index);
    uint16_t backwards(uint16_t index);

    void autocycle();
    void flipflop();

    bool is_wait_over(uint16_t interval);
    bool finished_waiting(uint16_t interval);

    void accelerate_decelerate_pattern(uint16_t draw_interval_initial, uint16_t delta_initial, uint16_t update_period, uint16_t genparam, void(ReAnimator::*pfp)(uint16_t, uint16_t, uint16_t(ReAnimator::*dfp)(uint16_t)), uint16_t(ReAnimator::*dfp)(uint16_t));
    void motion_blur(int8_t blur_num, uint16_t pos, uint16_t(ReAnimator::*dfp)(uint16_t));
    void fission();

    static int compare(const void * a, const void * b);

    Point serp2cart(uint8_t i);
    int16_t cart2serp(Point p);
    Point serp2cart_native(uint8_t i);
    int16_t cart2serp_native(Point p);
    uint16_t translate(uint16_t i, int8_t xi, int8_t yi, int8_t sx, int8_t sy, bool wrap, int8_t gap);
    void ntranslate(CRGBA in[], CRGBA out[], int8_t xi = 0, int8_t yi = 0, int8_t sx = 1, int8_t sy = 1, bool wrap = true, int8_t gap = 0);
    uint16_t mover(uint16_t i);

    //static void print_dt();

};


inline void noop_cb(uint8_t event) {
  __asm__ __volatile__ ("nop");
}
