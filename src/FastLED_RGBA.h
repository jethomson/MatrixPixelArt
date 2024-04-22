/* FastLED_RGBA
 * 
 * Hack to enable to save alpha alongside red, green, and blue in FastLED.
 *
 * Original code by Jim Bumgardner (http://krazydad.com).
 * Modified by David Madison (http://partsnotincluded.com).
 * Modified by Jonathan Thomson to use the fourth byte as alpha instead of white.
 * --most of the operators I added are modifications of operators found in FastLEDs pixeltypes.h
*/

#ifndef FastLED_RGBA_h
#define FastLED_RGBA_h

struct CRGBA  {
  union {
    struct {
      union {
        uint8_t g;
        uint8_t green;
      };
      union {
        uint8_t r;
        uint8_t red;
      };
      union {
        uint8_t b;
        uint8_t blue;
      };
      union {
        uint8_t a;
        uint8_t alpha;
      };
    };
    uint8_t raw[4];
  };

  CRGBA(){}

  CRGBA(uint8_t rd, uint8_t grn, uint8_t blu, uint8_t alph) {
    r = rd;
    g = grn;
    b = blu;
    a = alph;
  }

  inline void operator = (const CRGB c) __attribute__((always_inline)) { 
    this->r = c.r;
    this->g = c.g;
    this->b = c.b;
    this->a = 255;
  }
  
  inline void operator= (const uint32_t c) __attribute__((always_inline))
  {
    this->r = (c >> 16) & 0xFF;
    this->g = (c >>  8) & 0xFF;
    this->b = (c >>  0) & 0xFF;
    this->a = (c >> 24) & 0xFF;
  }

  inline explicit operator uint32_t() const {
    return (uint32_t{a} << 24) |
           (uint32_t{r} << 16) |
           (uint32_t{g} <<  8) |
            uint32_t{b};
  }

  inline explicit operator CRGB() const {
    //return uint32_t{0xff000000} |
    // this might should be the commented out line above instead.
    // unsure if FastLED ever uses the upper byte. FastLED definitely overwrites it with 0xFF.
    return (uint32_t{a} << 24) |  
           (uint32_t{r} << 16) |
           (uint32_t{g} <<  8) |
            uint32_t{b};
  }

};

inline __attribute__((always_inline)) bool operator== (const CRGBA& lhs, const uint32_t rhs) {
    //return (lhs.r == (rhs & 0xFF0000) >> 16) && (lhs.g == (rhs & 0x00FF00) >> 8) && (lhs.b == (rhs & 0x0000FF));
    return (lhs.r == (rhs >> 16) & 0xFF) && (lhs.g == (rhs >> 8) & 0xFF) && (lhs.b == (rhs & 0xFF));
}
 
inline __attribute__((always_inline)) bool operator!= (const CRGBA& lhs, const uint32_t rhs) {
    return !(lhs == rhs);
}

#endif
