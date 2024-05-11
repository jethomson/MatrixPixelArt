// slightly modified code from the WLED project used for converting a JSON array indices and colors to an LED matrix image.
// the following license applies to code in this file taken from the WLED project.
/*
MIT License

Copyright (c) 2016 Christian Schwinne

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

//#include <Arduino.h>
#include <stdint.h>
#include <FastLED.h>
#include "FastLED_RGBA.h"
#include "ArduinoJson-v6.h"

//color mangling macros from WLED wled.h
//modified slightly for alpha channel instead of white channel
#ifndef RGBA32
#define RGBA32(r,g,b,a) (uint32_t((byte(a) << 24) | (byte(r) << 16) | (byte(g) << 8) | (byte(b))))
#endif

// from WLED led.cpp
////scales the brightness with the briMultiplier factor
//byte scaledBri(byte in)
//{
//  uint16_t val = ((uint16_t)in*briMultiplier)/100;
//  if (val > 255) val = 255;
//  return (byte)val;
//}


// from WLED colors.cpp
//this uses RRGGBB / RRGGBBWW (RRGGBBAA) order
bool colorFromHexString(byte* rgb, const char* in) {
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
bool deserializeSegment(JsonObject root, CRGBA leds[], uint16_t leds_len)
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
