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

#ifndef PROJECT_H
#define PROJECT_H

#include <FastLED.h>
#include "FastLED_RGBA.h"
#include <stdint.h>
#include "Arduino.h"

#define LED_STRIP_VOLTAGE 5
#define LED_STRIP_MILLIAMPS 500

// storage locations for animated matrices and playlists.
// note the pathes are hardcoded in the HTML files, so changing these defines is not enough.
// do not put a / at the end
#define FILE_ROOT "/files"
#define IM_ROOT "/files/im"
#define PL_ROOT "/files/pl"
#define CM_ROOT "/files/cm"

#undef DEBUG_CONSOLE
#define DEBUG_CONSOLE Serial
#if defined DEBUG_CONSOLE && !defined DEBUG_PRINTLN
  #define DEBUG_BEGIN(x)     DEBUG_CONSOLE.begin (x)
  #define DEBUG_PRINT(x)     DEBUG_CONSOLE.print (x)
  #define DEBUG_PRINTDEC(x)     DEBUG_PRINT (x, DEC)
  #define DEBUG_PRINTLN(x)  DEBUG_CONSOLE.println (x)
  #define DEBUG_PRINTF(...) DEBUG_CONSOLE.printf(__VA_ARGS__)
//#else
//  #define DEBUG_BEGIN(x)
//  #define DEBUG_PRINT(x)
//  #define DEBUG_PRINTDEC(x)
//  #define DEBUG_PRINTLN(x)
//  #define DEBUG_PRINTF(...)
#endif


#define NUM_LEDS 256
#define MD 16 // width or height of matrix in number of LEDs
#define LED_STRIP_VOLTAGE 5
#define SOUND_VALUE_GAIN_INITIAL 2
#define HUE_ALIEN_GREEN 112


enum Pattern {            ORBIT = 0, THEATER_CHASE = 1,
                 RUNNING_LIGHTS = 2, SHOOTING_STAR = 3,
                 CYLON = 4, SOLID = 5, JUGGLE = 6, RIFFLE = 7,
                 MITOSIS = 8, BUBBLES = 9, SPARKLE = 10, MATRIX = 11,
                 WEAVE = 12, STARSHIP_RACE = 13, PUCK_MAN = 14, BALLS = 15, 
                 HALLOWEEN_FADE = 16, HALLOWEEN_ORBIT = 17, 
                 CHECKERBOARD = 18, BINARY_SYSTEM = 19, DYNAMIC_RAINBOW = 20, NONE = 21
                 //SOUND_RIBBONS = 17, SOUND_RIPPLE = 18, SOUND_BLOCKS = 19, SOUND_ORBIT = 20
                 };
enum Overlay {NO_OVERLAY = 0, GLITTER = 1, BREATHING = 2, CONFETTI = 3, FLICKER = 4, FROZEN_DECAY = 5};


#endif

