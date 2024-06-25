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
#include "lv_font.h"


enum LayerType {Pattern_t = 0, Accent_t = 1, Image_t = 2, Text_t = 3, Info_t = 4};

enum Pattern {
              DYNAMIC_RAINBOW = 0, SOLID = 1,
              ORBIT = 2, RUNNING_LIGHTS = 3,
              RIFFLE = 4, SPARKLE = 5, 
              WEAVE = 6, PENDULUM = 7, BINARY_SYSTEM = 8, 
              SHOOTING_STAR = 9, PUCK_MAN = 10, CYLON = 11,
              FUNKY = 12,
              NO_PATTERN = 50,
              THEATER_CHASE = 51, MITOSIS = 52,
              BUBBLES = 54, MATRIX = 55, STARSHIP_RACE = 56,
              BALLS = 57, HALLOWEEN_FADE = 58, HALLOWEEN_ORBIT = 59,
              CHECKERBOARD = 60, 
              DEMO = 99,
              //SOUND_RIBBONS = 60, SOUND_RIPPLE = 61, SOUND_BLOCKS = 62, SOUND_ORBIT = 63
             };

enum Overlay {NO_OVERLAY = 0, BREATHING = 1, FLICKER = 2, FROZEN_DECAY = 3, GLITTER = 50, CONFETTI = 51};

enum Info {TIME_12HR = 0, TIME_24HR = 1, DATE_MMDD = 2, DATE_DDMM = 3, TIME_12HR_DATE_MMDD = 4, TIME_24HR_DATE_DDMM = 5};


class ReAnimator {
    //enum Direction {STILL = 0, N = 1, NE = 2, E = 3, SE = 4, S = 5, SW = 6, W = 7, NW = 8};

    LayerType _ltype;
    int8_t _id ;

    String image_path;

    uint8_t MTX_NUM_ROWS = 0;
    uint8_t MTX_NUM_COLS = 0;
    uint16_t MTX_NUM_LEDS = 0;

    CRGBA* leds;
    //CRGBA leds[NUM_LEDS];
    //CRGBA tleds[NUM_LEDS]; // for use with ntranslate()

    uint8_t brightness;

    bool autocycle_enabled;
    uint32_t autocycle_previous_millis;
    uint32_t autocycle_interval;

    bool flipflop_enabled;
    uint32_t flipflop_previous_millis;
    uint32_t flipflop_interval;

    Pattern pattern;
    Pattern last_pattern_ran;
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
    CRGB proxy_color;
    bool proxy_color_set;

    void(*_cb)(uint8_t);

    class Freezer {
        ReAnimator &parent;
        const uint16_t m_after_all_black_pause = 500;
        const uint16_t m_failsafe_timeout = 3000;
        bool m_all_black = false;
        uint16_t m_frozen_duration = m_failsafe_timeout;
        bool m_frozen = false;
        uint32_t m_frozen_previous_millis = 0;
        uint32_t m_pm = 0;

      public:
        Freezer(ReAnimator &r);
        void timer(uint16_t freeze_interval);
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

    struct Starship {
        uint16_t distance;
        uint8_t color;
    };

    uint16_t dr_delta;

    //uint16_t previous_sample;
    //bool sample_peak;
    //uint16_t sample_average;
    //uint8_t sample_threshold;
    //uint16_t sound_value;
    //uint8_t sound_value_gain;

    bool fresh_image;

    struct Point {
      uint8_t x;
      uint8_t y;
    };

    lv_font_t* font;
    lv_font_t* clkfont;

    struct fstring {
      String s;
      int8_t vmargin = 0; // for centering text vertically
      uint8_t tracking = 1; // spacing between letters
    } ftext;


    uint16_t refresh_text_index;
    uint8_t shift_char_column;
    uint8_t shift_char_tracking; // spacing between letters

    // heading indicates which direction an object (image, patter, text, etc.) displayed on the matrix moves
    uint8_t heading;
    bool t_initial;
    bool t_visible;
    bool t_has_entered;
    int8_t dx;
    int8_t dy;

    uint32_t iwopm; // previous millis for is_wait_over()
    uint32_t fwpm ; // previous millis for finished_waiting()

  public:
    ReAnimator(uint8_t num_rows, uint8_t num_cols);
    ~ReAnimator() { free(leds); free(pm_puck_dots); }

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

    bool set_image(String fs_path, String* message = nullptr);
    void set_text(String s);
    void set_info(Info id);

    void set_color(CRGB *color);
    void set_color(CRGB color);

    void set_cb(void(*cb)(uint8_t));
    void set_heading(uint8_t h);

    //void set_sound_value_gain(uint8_t gain);

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
    void orbit(uint16_t draw_interval, int8_t delta);
    void theater_chase(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t));
    void general_chase(uint16_t draw_interval, uint16_t genparam, uint16_t(ReAnimator::*dfp)(uint16_t));
    //void accelerate_decelerate_theater_chase(uint16_t(ReAnimator::*dfp)(uint16_t));
    void running_lights(uint16_t draw_interval, uint16_t genparam, uint16_t(ReAnimator::*dfp)(uint16_t));
    //void accelerate_decelerate_running_lights(uint16_t(ReAnimator::*dfp)(uint16_t));
    void shooting_star(uint16_t draw_interval, uint8_t star_size, uint8_t star_trail_decay, uint8_t spm, uint16_t(ReAnimator::*dfp)(uint16_t));
    void cylon(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t));

    void solid(uint16_t draw_interval);
    void pendulum();
    void funky();
    void riffle();
    void mitosis(uint16_t draw_interval, uint8_t cell_size);
    void bubbles(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t));
    void sparkle(uint16_t draw_interval, bool random_color, uint8_t fade);
    void matrix(uint16_t draw_interval);
    void weave(uint16_t draw_interval);
    void starship_race(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t));
    void puck_man(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t));
    void bouncing_balls(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t));

    void halloween_colors_fade(uint16_t draw_interval);
    void halloween_colors_orbit(uint16_t draw_interval, int8_t delta);

    //void sound_ribbons(uint16_t draw_interval);
    //void sound_ripple(uint16_t draw_interval, bool trigger);
    //void sound_orbit(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t));
    //void sound_blocks(uint16_t draw_interval, bool trigger);

    void dynamic_rainbow(uint16_t draw_interval, uint16_t(ReAnimator::*dfp)(uint16_t));


// ++++++++++++++++++++++++++++++
// ++++++++++ OVERLAYS ++++++++++
// ++++++++++++++++++++++++++++++
    void breathing(uint16_t interval);
    void flicker(uint16_t interval);
    void glitter(uint16_t chance_of_glitter);
    void fade_randomly(uint8_t chance_of_fade, uint8_t decay);


// ++++++++++++++++++++++++++++++
// +++++++++++ IMAGE ++++++++++++
// ++++++++++++++++++++++++++++++
  bool load_image_from_file(String fs_path, String* message = nullptr);


// ++++++++++++++++++++++++++++++
// ++++++++++++ TEXT ++++++++++++
// ++++++++++++++++++++++++++++++
    uint8_t get_text_center(String s);
    bool shift_char(char c, int8_t vmargin = 0);
    //void matrix_char(char c);
    //void matrix_text(String s);


// ++++++++++++++++++++++++++++++
// ++++++++++++ INFO ++++++++++++
// ++++++++++++++++++++++++++++++
    void setup_clock();
    void refresh_date_time(uint16_t draw_interval);


// ++++++++++++++++++++++++++++++
// ++++++++++ HELPERS +++++++++++
// ++++++++++++++++++++++++++++++
    void fadeToBlackBy(CRGBA* leds, uint16_t num_leds, uint8_t fadeBy);
    void fill_solid(struct CRGBA* targetArray, int numToFill, const struct CRGB& color);

    uint16_t forwards(uint16_t index);
    uint16_t backwards(uint16_t index);

    void autocycle();
    void flipflop();

    bool is_wait_over(uint16_t interval);
    bool finished_waiting(uint16_t interval);

    void accelerate_decelerate_pattern(uint16_t draw_interval_initial, uint16_t delta_initial, uint16_t update_period, uint16_t genparam, void(ReAnimator::*pfp)(uint16_t, uint16_t, uint16_t(ReAnimator::*dfp)(uint16_t)), uint16_t(ReAnimator::*dfp)(uint16_t));
    //void process_sound();
    void motion_blur(int8_t blur_num, uint16_t pos, uint16_t(ReAnimator::*dfp)(uint16_t));
    void fission();

    static int compare(const void * a, const void * b);

    Point serp2cart(uint8_t i);
    int16_t cart2serp(Point p);
    void flip(CRGB sm[], bool dim);
    uint16_t translate(uint16_t i, int8_t xi, int8_t yi, int8_t sx, int8_t sy, bool wrap, int8_t gap);
    void ntranslate(CRGBA in[], CRGBA out[], int8_t xi = 0, int8_t yi = 0, int8_t sx = 1, int8_t sy = 1, bool wrap = true, int8_t gap = 0);
    uint16_t mover(uint16_t i);

    //void print_dt();

};


inline void noop_cb(uint8_t event) {
  __asm__ __volatile__ ("nop");
}
