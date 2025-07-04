/* FastLED_RGBA
 * 
 * Hack to enable to save alpha alongside red, green, and blue in FastLED.
 *
 * Original code by Jim Bumgardner (http://krazydad.com).
 * Modified by David Madison (http://partsnotincluded.com).
 * Modified by Jonathan Thomson to use the fourth byte as alpha instead of white.
 * --most of the operators I added are modifications of operators found in FastLEDs pixeltypes.h
*/

#pragma once

struct CRGBA  {
  union {
    struct {
      union {
        uint8_t r;
        uint8_t red;
      };
      union {
        uint8_t g;
        uint8_t green;
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

  CRGBA(CRGB c) {
    r = c.r;
    g = c.g;
    b = c.b;
    a = 255;
  }

  // setting with HTMLColorCode (e.g. CRGB::White, CRGB::Red, 0x00FF00) can call this
  // use CRGBA color codes to avoid getting a transparent color
  CRGBA(uint32_t c) {
    r = (c >> 16) & 0xFF;
    g = (c >>  8) & 0xFF;
    b = (c >>  0) & 0xFF;
    a = (c >> 24) & 0xFF;
    // the upper bits of an HTMLColorCode are always 0 which results in a completely
    // transparent color if we use this for the alpha.
    // since most of the time we want an HTMLColorCode to result in a completely
    // opaque color, substitue 0xFF for 0x00.
    //a = (c >> 24) ? (c >> 24) & 0xFF : 0xFF;
  }


  CRGBA(uint8_t rd, uint8_t grn, uint8_t blu, uint8_t alph) {
    r = rd;
    g = grn;
    b = blu;
    a = alph;
  }

  inline void operator= (const CRGB c) __attribute__((always_inline)) { 
    this->r = c.r;
    this->g = c.g;
    this->b = c.b;
    this->a = 255; // must set alpha when setting color with CRGB otherwise color will be completely transaparent
  }
  
  // setting with HTMLColorCode (e.g. CRGB::White, CRGB::Red, 0x00FF00) can call this
  // use CRGBA color codes to avoid getting a transparent color
  inline void operator= (const uint32_t c) __attribute__((always_inline)) {
    this->r = (c >> 16) & 0xFF;
    this->g = (c >>  8) & 0xFF;
    this->b = (c >>  0) & 0xFF;
    this->a = (c >> 24) & 0xFF;
    // the upper bits of an HTMLColorCode are always 0 which results in a completely
    // transparent color if we use this for the alpha.
    // since most of the time we want an HTMLColorCode to result in a completely
    // opaque color, substitue 0xFF for 0x00.
    //this->a = (c >> 24) ? (c >> 24) & 0xFF : 0xFF;
  }

  inline explicit operator uint32_t() const {
    return (uint32_t{a} << 24) |
           (uint32_t{r} << 16) |
           (uint32_t{g} <<  8) |
            uint32_t{b};
  }

  inline explicit operator CRGB() const {
    return uint32_t{0xFF000000} |
            (uint32_t{r} << 16) |
            (uint32_t{g} <<  8) |
             uint32_t{b};
  }

  inline CRGBA(const CHSV& rhs) __attribute__((always_inline)) {
    CRGB c;
    hsv2rgb_rainbow(rhs, c);
    this->r = c.r;
    this->g = c.g;
    this->b = c.b;
    this->a = 255;
  }

  inline CRGBA& operator= (const CHSV& rhs) __attribute__((always_inline)) {
    CRGB c;
    hsv2rgb_rainbow(rhs, c);
    this->r = c.r;
    this->g = c.g;
    this->b = c.b;
    this->a = 255;
    return *this;
  }

  inline CRGBA& operator+= (const CRGBA& rhs) {
    r = qadd8(r, rhs.r);
    g = qadd8(g, rhs.g);
    b = qadd8(b, rhs.b);
    a = qadd8(a, rhs.a);
    return *this;
  }

  inline CRGBA& fadeToBlackBy (uint8_t fadefactor) {
    nscale8x3(r, g, b, 255 - fadefactor);
    a = scale8(a, 255 - fadefactor);
    return *this;
  }

  // when color substitution is involved we do not want to alter the proxy color
  // so fade by decreasing the transparency.
  // this will mostly look the same as fadeToBlackBy() but can lead to overlapping
  // colors look more white than they would if fadeToBlackBy() is used.
  inline CRGBA& fadeToTransparentBy (uint8_t fadefactor) {
    a = scale8(a, 255 - fadefactor);
    if (a == 0) {
      this->r = 0;
      this->g = 0;
      this->b = 0;
    }
    return *this;
  }

  inline CRGBA& operator|= (const CRGB& rhs) {
    if (rhs.r > r) r = rhs.r;
    if (rhs.g > g) g = rhs.g;
    if (rhs.b > b) b = rhs.b;
    a = 255;
    return *this;
  }
 
  inline CRGBA& operator|= (uint8_t d) {
    if (d > r) r = d;
    if (d > g) g = d;
    if (d > b) b = d;
    a = 255;
    return *this;
  }

  typedef enum {
    Transparent=0x00000000,
    OpaqueBlack=0XFF000000,
    AliceBlue=0xFFF0F8FF,
    Amethyst=0xFF9966CC,
    AntiqueWhite=0xFFFAEBD7,
    Aqua=0xFF00FFFF,
    Aquamarine=0xFF7FFFD4,
    Azure=0xFFF0FFFF,
    Beige=0xFFF5F5DC,
    Bisque=0xFFFFE4C4,
    Black=0xFF000000,
    BlanchedAlmond=0xFFFFEBCD,
    Blue=0xFF0000FF,
    BlueViolet=0xFF8A2BE2,
    Brown=0xFFA52A2A,
    BurlyWood=0xFFDEB887,
    CadetBlue=0xFF5F9EA0,
    Chartreuse=0xFF7FFF00,
    Chocolate=0xFFD2691E,
    Coral=0xFFFF7F50,
    CornflowerBlue=0xFF6495ED,
    Cornsilk=0xFFFFF8DC,
    Crimson=0xFFDC143C,
    Cyan=0xFF00FFFF,
    DarkBlue=0xFF00008B,
    DarkCyan=0xFF008B8B,
    DarkGoldenrod=0xFFB8860B,
    DarkGray=0xFFA9A9A9,
    DarkGrey=0xFFA9A9A9,
    DarkGreen=0xFF006400,
    DarkKhaki=0xFFBDB76B,
    DarkMagenta=0xFF8B008B,
    DarkOliveGreen=0xFF556B2F,
    DarkOrange=0xFFFF8C00,
    DarkOrchid=0xFF9932CC,
    DarkRed=0xFF8B0000,
    DarkSalmon=0xFFE9967A,
    DarkSeaGreen=0xFF8FBC8F,
    DarkSlateBlue=0xFF483D8B,
    DarkSlateGray=0xFF2F4F4F,
    DarkSlateGrey=0xFF2F4F4F,
    DarkTurquoise=0xFF00CED1,
    DarkViolet=0xFF9400D3,
    DeepPink=0xFFFF1493,
    DeepSkyBlue=0xFF00BFFF,
    DimGray=0xFF696969,
    DimGrey=0xFF696969,
    DodgerBlue=0xFF1E90FF,
    FireBrick=0xFFB22222,
    FloralWhite=0xFFFFFAF0,
    ForestGreen=0xFF228B22,
    Fuchsia=0xFFFF00FF,
    Gainsboro=0xFFDCDCDC,
    GhostWhite=0xFFF8F8FF,
    Gold=0xFFFFD700,
    Goldenrod=0xFFDAA520,
    Gray=0xFF808080,
    Grey=0xFF808080,
    Green=0xFF008000,
    GreenYellow=0xFFADFF2F,
    Honeydew=0xFFF0FFF0,
    HotPink=0xFFFF69B4,
    IndianRed=0xFFCD5C5C,
    Indigo=0xFF4B0082,
    Ivory=0xFFFFFFF0,
    Khaki=0xFFF0E68C,
    Lavender=0xFFE6E6FA,
    LavenderBlush=0xFFFFF0F5,
    LawnGreen=0xFF7CFC00,
    LemonChiffon=0xFFFFFACD,
    LightBlue=0xFFADD8E6,
    LightCoral=0xFFF08080,
    LightCyan=0xFFE0FFFF,
    LightGoldenrodYellow=0xFFFAFAD2,
    LightGreen=0xFF90EE90,
    LightGrey=0xFFD3D3D3,
    LightPink=0xFFFFB6C1,
    LightSalmon=0xFFFFA07A,
    LightSeaGreen=0xFF20B2AA,
    LightSkyBlue=0xFF87CEFA,
    LightSlateGray=0xFF778899,
    LightSlateGrey=0xFF778899,
    LightSteelBlue=0xFFB0C4DE,
    LightYellow=0xFFFFFFE0,
    Lime=0xFF00FF00,
    LimeGreen=0xFF32CD32,
    Linen=0xFFFAF0E6,
    Magenta=0xFFFF00FF,
    Maroon=0xFF800000,
    MediumAquamarine=0xFF66CDAA,
    MediumBlue=0xFF0000CD,
    MediumOrchid=0xFFBA55D3,
    MediumPurple=0xFF9370DB,
    MediumSeaGreen=0xFF3CB371,
    MediumSlateBlue=0xFF7B68EE,
    MediumSpringGreen=0xFF00FA9A,
    MediumTurquoise=0xFF48D1CC,
    MediumVioletRed=0xFFC71585,
    MidnightBlue=0xFF191970,
    MintCream=0xFFF5FFFA,
    MistyRose=0xFFFFE4E1,
    Moccasin=0xFFFFE4B5,
    NavajoWhite=0xFFFFDEAD,
    Navy=0xFF000080,
    OldLace=0xFFFDF5E6,
    Olive=0xFF808000,
    OliveDrab=0xFF6B8E23,
    Orange=0xFFFFA500,
    OrangeRed=0xFFFF4500,
    Orchid=0xFFDA70D6,
    PaleGoldenrod=0xFFEEE8AA,
    PaleGreen=0xFF98FB98,
    PaleTurquoise=0xFFAFEEEE,
    PaleVioletRed=0xFFDB7093,
    PapayaWhip=0xFFFFEFD5,
    PeachPuff=0xFFFFDAB9,
    Peru=0xFFCD853F,
    Pink=0xFFFFC0CB,
    Plaid=0xFFCC5533,
    Plum=0xFFDDA0DD,
    PowderBlue=0xFFB0E0E6,
    Purple=0xFF800080,
    Red=0xFFFF0000,
    RosyBrown=0xFFBC8F8F,
    RoyalBlue=0xFF4169E1,
    SaddleBrown=0xFF8B4513,
    Salmon=0xFFFA8072,
    SandyBrown=0xFFF4A460,
    SeaGreen=0xFF2E8B57,
    Seashell=0xFFFFF5EE,
    Sienna=0xFFA0522D,
    Silver=0xFFC0C0C0,
    SkyBlue=0xFF87CEEB,
    SlateBlue=0xFF6A5ACD,
    SlateGray=0xFF708090,
    SlateGrey=0xFF708090,
    Snow=0xFFFFFAFA,
    SpringGreen=0xFF00FF7F,
    SteelBlue=0xFF4682B4,
    Tan=0xFFD2B48C,
    Teal=0xFF008080,
    Thistle=0xFFD8BFD8,
    Tomato=0xFFFF6347,
    Turquoise=0xFF40E0D0,
    Violet=0xFFEE82EE,
    Wheat=0xFFF5DEB3,
    White=0xFFFFFFFF,
    WhiteSmoke=0xFFF5F5F5,
    Yellow=0xFFFFFF00,
    YellowGreen=0xFF9ACD32,

    // LED RGB color that roughly approximates
    // the color of incandescent fairy lights,
    // assuming that you're using FastLED
    // color correction on your LEDs (recommended).
    FairyLight=0xFFFFE42D,

    // If you are using no color correction, use this
    FairyLightNCC=0xFFFF9D2A

  } HTMLColorCode;
};


inline __attribute__((always_inline)) bool operator== (const CRGBA& lhs, const uint32_t rhs) {
    return (lhs.a == (uint8_t)(rhs >> 24) & 0xFF) && (lhs.r == (uint8_t)(rhs >> 16) & 0xFF) && (lhs.g == (uint8_t)(rhs >> 8) & 0xFF) && (lhs.b == (uint8_t)(rhs & 0xFF));

}
 
inline __attribute__((always_inline)) bool operator!= (const CRGBA& lhs, const uint32_t rhs) {
    return !(lhs == rhs);
}

inline __attribute__((always_inline)) bool operator== (const CRGBA& lhs, const CRGB& rhs) {
    return (lhs.r == rhs.r) && (lhs.g == rhs.g) && (lhs.b == rhs.b);
}
 
inline __attribute__((always_inline)) bool operator!= (const CRGBA& lhs, const CRGB& rhs) {
    return !(lhs == rhs);
}
