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

//#include <FastLED.h>
//#include "FastLED_RGBA.h"
//#include <stdint.h>
//#include "Arduino.h"

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

#define SOUND_VALUE_GAIN_INITIAL 2
#define HUE_ALIEN_GREEN 112

inline String form_path(String type, String id) {
  String fs_path = "";
  if (type == "im") {
    fs_path += IM_ROOT;
  }
  else if (type == "cm") {
    fs_path += CM_ROOT;
  }
  else if (type == "pl") {
    fs_path += PL_ROOT;
  }
  else {
    return fs_path;
  }
  fs_path += "/";
  fs_path += id;
  fs_path += ".json";

  return fs_path;
}

/*
inline String form_path(String root, String id) {
  String fs_path = "";
  String param_path_top_dir = "/";
  if (id.indexOf('/') == 0) {
    id.remove(0,1);
  }
  param_path_top_dir += id.substring(0, id.indexOf('/'));

  if (param_path_top_dir != root) {
    fs_path = root;
  }

  fs_path += "/";
  fs_path += id;
  fs_path += ".json";

  return fs_path;
}
*/

#endif

