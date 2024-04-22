//#include <Arduino.h>
#include <FS.h>
//#include <ESPmDNS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
//#include <SPIFFSEditor.h>
#include <LittleFS.h>
//#include <SPI.h>

#include <FastLED.h>
#include "FastLED_RGBA.h"

#include "src/dependencies/json/ArduinoJson-v6.h"
#include "minjson.h"

#include "project.h"
#include "ReAnimator.h"

//#include "credentials.h" // set const char *wifi_ssid and const char *wifi_password in include/credentials.h
#include "credentials_private.h"

#define DATA_PIN 16
#define COLOR_ORDER GRB
#define LED_STRIP_VOLTAGE 5
#define LED_STRIP_MILLIAMPS 500

#define TRANSPARENT (uint32_t)0x424242
//#define COLORSUB (uint32_t)0x000900
#define COLORSUB (uint32_t)0x004200
#define MD 16 // width or height of matrix in number of LEDs


// storage locations for animated matrices and playlists.
// note the pathes are hardcoded in the HTML files, so changing these defines is not enough.
// do not put a / at the end
#define FILE_ROOT "/files"
#define AM_ROOT "/files/am"
#define PL_ROOT "/files/pl"


AsyncWebServer web_server(80);

// currently there is a background layer, image layer, and overlay layer
// for now the background layer is the pattern layer pl[], and the overlay affect is achieve by manipulating leds[] after the other layers have been written to it.
// pl - pattern layer. currently background. could possibly dynamically change the z height of where a pattern is drawn such that it is sometimes above an image, text, etc.
// il - image layer. pixel art is written here. if the image is completely transparent then only effects will be shown.
// al - accent layer. currently accent effects are implemented for the overlay, but they could possibly be put underneath. for example, an image could sparkle from underneath. twinkling stars?
// tl - text layer. not used yet.
CRGB leds[NUM_LEDS] = {0}; // output
CRGB pl[NUM_LEDS] = {0}; // pattern layer. written to by GlowSerum.
//CRGB il[NUM_LEDS] = {0}; // image layer. pixel art is written to this layer.
CRGB tl[NUM_LEDS] = {0}; // text layer. text or anything meant to be shifter is written to this layer.


// FastLED with RGBA
CRGBA il[NUM_LEDS];
//CRGB *ledsRGB = (CRGB *) &il[0];


uint8_t gmax_brightness = 255;
//const uint8_t gmin_brightness = 2;
uint8_t gdynamic_hue = 0;
uint8_t gstatic_hue = HUE_ALIEN_GREEN;
uint8_t grandom_hue = 0;
uint8_t gimage_layer_alpha = 255;
bool gdemo_enabled = false;

ReAnimator GlowSerum(pl, &gdynamic_hue, LED_STRIP_MILLIAMPS);
ReAnimator MiskaTonic(leds, &gdynamic_hue, LED_STRIP_MILLIAMPS);

bool gplaylist_enabled = false;
bool gpattern_layer_enable = true;

struct {
  String type;
  String filename;
} gnextup;



void create_dirs(String path);
void list_files(File dir, String parent);
void handle_file_list(void);
void delete_files(String name, String parent);
void handle_delete_list(void);
String form_path(String root, String id);
void set_alfx(uint8_t alfx);
void set_plfx(uint8_t plfx);
bool save_data(String fs_path, String json, String* message);
bool load_matrix_from_json(String json, String* message);
bool load_matrix_from_file(String fs_path, String* message);
bool load_matrix_from_playlist(String id, String* message);
void demo(void);
void web_server_initiate(void);

void pac_man_cb(uint8_t event) {
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

void noop_cb(uint8_t event) {
  __asm__ __volatile__ ("nop");
}

struct Point{
  uint8_t x;
  uint8_t y;
};
typedef struct Point Point;

Point serp2cart(uint8_t i) {
  const uint8_t rl = 16;
  Point p;
  p.y = i/rl;
  p.x = (p.y % 2) ? (rl-1) - (i % rl) : i % rl;
  return p;
}

int16_t cart2serp(Point p) {
  const uint8_t rl = 16;
  int16_t i = (p.y % 2) ? (rl*p.y + rl-1) - p.x : rl*p.y + p.x;
  return i;
}


void flip(CRGB sm[NUM_LEDS], bool dim) {
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


// only works for moving one pixel at a time
void move(CRGB sm[NUM_LEDS], bool dim, uint8_t d) {
  const uint8_t rl = 16;
  if ((d % rl) == 0) {
    return;
  }

  Point p1;
  Point p2;
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
      /*
      if (k+d < rl) {
        sm[cart2serp(p2)] = sm[cart2serp(p1)];
      }
      else {
        sm[cart2serp(p1)] = TRANSPARENT;
      }
      */
    }
    sm[cart2serp(p1)] = tmp;
  }
}


enum Direction {RTL = 0, LTR = 1, DOWN = 2, UP = 3};
void shift(CRGB in[NUM_LEDS], CRGB out[NUM_LEDS], Direction t=RTL, bool loop=false, uint8_t gap=0) {
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

  Point p1;
  Point p2;

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

  /*
  static uint8_t tt = 0;
  if (tt % 2) {
      dim = 0;
      dir = 1;
  }
  else  {
      dim = 1;
      dir = 0;
  }
  tt++;
  */

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




void position_OLD(CRGB in[NUM_LEDS], CRGB out[NUM_LEDS], int8_t x0 = 0, int8_t y0 = 0, bool wrap=true, uint8_t gap=0) {
  const uint8_t rl = 16;

  //if (!wrap && (x0 <= -MD || y0 <= -MD || x0 > MD || y0 > MD)) {
  //  return;
  //}

  Point p1;
  Point p2;

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
void position(CRGB in[NUM_LEDS], CRGB out[NUM_LEDS], int8_t xi = 0, int8_t yi = 0, int8_t sx = 1, int8_t sy = 1, bool wrap = true, int8_t gap = 0) {
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
  Point p1;
  Point p2;

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


void posmove(CRGB in[NUM_LEDS], CRGB out[NUM_LEDS], int8_t xi = 0, int8_t yi = 0, int8_t sx = 1, int8_t sy = 1, bool wrap=true, uint8_t gap=0) {
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


void create_dirs(String path) {
  int f = path.indexOf('/');
  if (f == -1) {
    return;
  }
  if (f == 0) {
    path = path.substring(1);
  }
  if (!path.endsWith("/")) {
    path += '/';
  }
  //DEBUG_PRINTLN(path);

  f = path.indexOf('/');

  while (f != -1) {
    String dir = "/";
    dir += path.substring(0, f);
    if (!LittleFS.exists(dir)) {
      LittleFS.mkdir(dir);
    }
    //DEBUG_PRINT("create_dirs: ");
    //DEBUG_PRINTLN(dir);
    int nf = path.substring(f+1).indexOf('/');
    if (nf != -1) {
      f += nf + 1;
    }
    else {
      f = -1;
      }
  }
}


String gfile_list_tmp;
String gfile_list;
String gfile_list_json_tmp;
String gfile_list_json;
// NOTE: An empty folder will not be added when building a littlefs image.
// Empty folders will not be created when uploaded either.
void list_files(File dir, String parent) {
  String path = parent;
  if (!parent.endsWith("/")) {
    path += "/";
  }
  path += dir.name();

  //DEBUG_PRINT("list_files(): ");
  //DEBUG_PRINTLN(path);

  gfile_list_tmp += path;
  gfile_list_tmp += "/\n";

  gfile_list_json_tmp += "{\"type\":\"dir\",\"name\":\"";
  gfile_list_json_tmp += path;
  gfile_list_json_tmp += "\"},";

  while (File entry = dir.openNextFile()) {
    if (entry.isDirectory()) {
      list_files(entry, path);
      entry.close();
    }
    else {
      gfile_list_tmp += path;
      gfile_list_tmp += "/";
      gfile_list_tmp += entry.name();
      gfile_list_tmp += "\t";
      gfile_list_tmp += entry.size();
      gfile_list_tmp += "\n";

      gfile_list_json_tmp += "{\"type\":\"file\",\"path\":\"";
      gfile_list_json_tmp += path;
      gfile_list_json_tmp += "/";
      gfile_list_json_tmp += entry.name();
      gfile_list_json_tmp += "\",";
      gfile_list_json_tmp += "\"name\":\"";
      gfile_list_json_tmp += entry.name();
      gfile_list_json_tmp += "\",";
      gfile_list_json_tmp += "\"size\":";
      gfile_list_json_tmp += entry.size();
      gfile_list_json_tmp += "},";

      entry.close();
    }
  }
}


bool gfile_list_needs_refresh = true;
void handle_file_list(void) {
  // refreshing file list is slow and can cause playlists with short durations to lag
  // therefore use a flag to indicate refresh is needed instead of refreshing periodically using a timer
  if (gfile_list_needs_refresh) {
    gfile_list_needs_refresh = false;
    gfile_list_tmp = "";
    // cannot prevent "open(): /littlefs/images does not exist, no permits for creation" message
    // the abscense of FILE_ROOT is not an error.
    // tried using exists() before open to prevent message but exists() calls open()
    File file_root = LittleFS.open(FILE_ROOT);
    //File file_root = LittleFS.open("/");  // use this to see all files for debugging
    if (file_root) {
      list_files(file_root, "/");
      file_root.close();
    }
    gfile_list = gfile_list_tmp;
    gfile_list_tmp = "";

    gfile_list_json_tmp.setCharAt(gfile_list_json_tmp.length()-1, ']');
    gfile_list_json = "[" + gfile_list_json_tmp;
    gfile_list_json_tmp = "";
  }
}


String gdelete_list;
void delete_files(String name, String parent) {
  String path = parent;
  if (!parent.endsWith("/")) {
    path += "/";
  }
  path += name;

  File entry = LittleFS.open(path);
  if (entry) {
    if (entry.isDirectory()) {
      while (File e = entry.openNextFile()) {
        String ename = e.name();
        e.close();
        delay(1);
        delete_files(ename, path);
      }
      entry.close();
      delay(1);
      LittleFS.rmdir(path);
    }
    else {
      entry.close();
      delay(1);
      LittleFS.remove(path);
    }
  }
}


void handle_delete_list(void) {
  if (gdelete_list != "") {
    //DEBUG_PRINT("gdelete_list: ");
    //DEBUG_PRINTLN(gdelete_list);

    int f = gdelete_list.indexOf('\n');
    while (gdelete_list != "") {
      String name = gdelete_list.substring(0, f);
      delete_files(name, "/");
      if (f+1 < gdelete_list.length()) {
        gdelete_list = gdelete_list.substring(f+1);
        f = gdelete_list.indexOf('\n');
      }
      else {
        gdelete_list = "";
      }
    }
    gfile_list_needs_refresh = true;
  }
}


String form_path(String root, String id) {
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

  return fs_path;
}


//accent layer effects
void set_alfx(uint8_t alfx) {
  switch(alfx) {
    default:
      // fall through to next case
    case 0:
      MiskaTonic.set_overlay(NO_OVERLAY, false);
      break;
    case 1:
      MiskaTonic.set_overlay(GLITTER, false);
      break;
    case 2:
      MiskaTonic.set_overlay(CONFETTI, false);
      break;
    case 3:
      MiskaTonic.set_overlay(FLICKER, false);
      break;
    case 4:
      MiskaTonic.set_overlay(FROZEN_DECAY, false);
      break;
  }
}


// pattern layer effects
void set_plfx(uint8_t plfx) {
  gpattern_layer_enable = true;
  gdemo_enabled = false;
  switch(plfx) {
    default:
      // fall through to next case
      GlowSerum.set_cb(&noop_cb);
    case 0:
      // NONE does nothing. not even clear the LEDs, so set them to black once.
      // this is preferable because we don't want the NONE pattern setting the LEDs to black every time it is ran.
      fill_solid(pl, NUM_LEDS, CRGB::Black);
      GlowSerum.set_pattern(NONE);
      gpattern_layer_enable = false;
      break;
    case 1:
      //fill_rainbow(leds, NUM_LEDS, gdynamic_hue, 2);
      GlowSerum.set_pattern(DYNAMIC_RAINBOW);
      break;
    case 2:
      //fill_solid(leds, NUM_LEDS, CHSV(gdynamic_hue, 255, 255));
      GlowSerum.set_pattern(SOLID);
      //GlowSerum.set_overlay(CONFETTI, false);
      //GlowSerum.set_overlay(FROZEN_DECAY, false);
      break;
    case 3:
      GlowSerum.set_pattern(ORBIT);
      break;
    case 4:
      GlowSerum.set_pattern(RUNNING_LIGHTS);
      break;
    case 5:
      GlowSerum.set_pattern(JUGGLE);
      break;
    case 6:
      GlowSerum.set_pattern(SPARKLE);
      break;
    case 7:
      GlowSerum.set_pattern(WEAVE);
      break;
    case 8:
      GlowSerum.set_pattern(CHECKERBOARD);
      break;
    case 9:
      GlowSerum.set_pattern(BINARY_SYSTEM);
      //GlowSerum.set_overlay(FROZEN_DECAY, false);
      break;
    case 10:
      GlowSerum.set_pattern(SHOOTING_STAR);
      break;
    case 11:
      GlowSerum.set_pattern(PAC_MAN);
      GlowSerum.set_cb(&pac_man_cb);
      break;
    case 12:
      GlowSerum.set_pattern(CYLON);
      break;
    case 99:
      gdemo_enabled = true;
      break;
  }
}


bool save_data(String fs_path, String json, String* message = nullptr) {
  if (fs_path == "") {
    if (message) {
      *message = F("save_data(): Filename is empty. Data not saved.");
    }
    return false;
  }

  create_dirs(fs_path.substring(0, fs_path.lastIndexOf("/")+1));
  File f = LittleFS.open(fs_path, "w");
  if (f) {
    //noInterrupts();
    f.print(json);
    delay(1);
    f.close();
    //interrupts();
  }
  else {
    if (message) {
      *message = F("save_data(): Could not open file.");
    }
    return false;
  }

  if (message) {
    *message = F("save_data(): Data saved.");
  }
  return true;
}


bool load_matrix_from_json(String json, String* message = nullptr) {
  //const size_t CAPACITY = JSON_OBJECT_SIZE(6) + JSON_ARRAY_SIZE(360);
  //StaticJsonDocument<CAPACITY> doc;
  DynamicJsonDocument doc(8192);

  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    //Serial.print("deserializeJson() failed: ");
    //Serial.println(error.c_str());
    if (message) {
      *message = F("load_matrix_from_json: deserializeJson() failed.");
    }
    return false;
  }

  JsonObject object = doc.as<JsonObject>();
  //const char* alfx = object["alfx"]; // const char* doesn't work with Serial.print() perhaps?
  //String alfx = object["alfx"];

  set_alfx(object["alfx"]);
  set_plfx(object["plfx"]);

  gimage_layer_alpha = object["ila"];

  //for (uint16_t i = 0; i < NUM_LEDS; i++) il[i] = 0;
  deserializeSegment(object, il, NUM_LEDS);

  String plfx = object["plfx"];
  //Serial.print("plfx: ");
  //Serial.println(plfx);
  //Serial.println("load_matrix_from_json finished");
  //if (message) {
  // *message = F("load_matrix_from_json: finished.");
  //}

  //??? actual success ??? need to check deserializeSegment()
  return true;
}


bool load_matrix_from_file(String fs_path, String* message = nullptr) {
  File file = LittleFS.open(fs_path, "r");
  
  if(!file){
    if (message) {
      *message = F("load_matrix_from_file(): File not found.");
    }
    return false;
  }

  if (file.available()) {
    load_matrix_from_json(file.readString());
  }
  file.close();

  if (message) {
    *message = F("load_matrix_from_file(): Matrix loaded.");
  }
  return true;
}


bool load_matrix_from_playlist(String id = "", String* message = nullptr) {
  static String _id;
  static uint32_t pm = 0;
  static uint32_t am_duration = 0;
  static uint8_t i = 0;
  bool refresh_needed = false;

  if (id != "") {
    gplaylist_enabled = true;
    _id = id;
    pm = 0;
    am_duration = 0;
    i = 0;
  }

  if (_id != "" && (millis()-pm) > am_duration) {
    pm = millis();
    am_duration = 1000; // set to a safe value which will be replaced below

    //String fs_path = form_path(_id, PL_ROOT);
    //File file = LittleFS.open(form_path(_id, PL_ROOT), "r");
    File file = LittleFS.open(_id, "r");
    if (file && file.available()) {
      //Serial.print("i: ");
      //Serial.println(i);

      String json = file.readString();
      file.close();

      // allocate the memory for the document
      //const size_t CAPACITY = JSON_OBJECT_SIZE(6) + JSON_ARRAY_SIZE(60);
      //StaticJsonDocument<CAPACITY> doc;
      DynamicJsonDocument doc(2048);

      DeserializationError error = deserializeJson(doc, json);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        gplaylist_enabled = false;
        refresh_needed = false;
        if (message) {
          *message = "load_matrix_from_playlist(): deserializeJson failed.";
        }
        return refresh_needed;
      }

      JsonObject object = doc.as<JsonObject>();
      JsonArray arr = object[F("am")];
      if (!arr.isNull() && arr.size() > 0) {
        if(arr[i].is<JsonVariant>()) {
          JsonVariant am = arr[i];
          //String msg = "load_matrix_from_playlist(): playlist disabled.";
          if(load_matrix_from_file(am[F("id")])) {
            if(am[F("d")].is<JsonInteger>()) {
              am_duration = am[F("d")];
            }
            //Serial.print("duration: ");
            //Serial.println(am_duration);
            refresh_needed = true;
          }
          else {
            am_duration = 0;
          }
        }
        i = (i+1) % arr.size();
      }
      else {
        gplaylist_enabled = false;
        refresh_needed = false;
        if (message) {
          *message = "load_matrix_from_playlist(): Invalid playlist.";
        }
      }
    }
  }
  return refresh_needed;
}


void demo() {
  const uint8_t PATTERNS_NUM = 21;

  const Pattern patterns[PATTERNS_NUM] = {DYNAMIC_RAINBOW, SOLID, ORBIT, RUNNING_LIGHTS,
                                          JUGGLE, SPARKLE, WEAVE, CHECKERBOARD, BINARY_SYSTEM,
                                          SOLID, SOLID, SOLID, SOLID, DYNAMIC_RAINBOW,
                                          SHOOTING_STAR, //MITOSIS, BUBBLES, MATRIX,
                                          BALLS, CYLON,
                                          //STARSHIP_RACE
                                          //PAC_MAN, // PAC_MAN crashes ???
                                          /*SOUND_BLOCKS, SOUND_BLOCKS, SOUND_RIBBONS, SOUND_RIBBONS,
                                          SOUND_ORBIT, SOUND_ORBIT, SOUND_RIPPLE, SOUND_RIPPLE*/};



  const Overlay overlays[PATTERNS_NUM] = {NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY,
                                          NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY,
                                          GLITTER, BREATHING, CONFETTI, FLICKER, FROZEN_DECAY,
                                          NO_OVERLAY, //NO_OVERLAY, NO_OVERLAY, NO_OVERLAY,
                                          NO_OVERLAY, NO_OVERLAY,
                                          //NO_OVERLAY
                                          //NO_OVERLAY
                                          /*NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY,
                                          NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY*/};

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


void web_server_initiate() {

  web_server.serveStatic("/", LittleFS, "/").setDefaultFile("html/index.html");

  web_server.on("/art.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/art.html");
  });

  web_server.on("/playlist_maker.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/playlist_maker.html");
  });

  web_server.on("/play.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/play.html");
  });

  web_server.on("/remove.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/remove.html");
  });


  // perhaps use the processor to replace references to AM_ROOT and PL_ROOT in html files
  // this is problematic because the $ is used for formatted strings in javascript
  //web_server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
  //  //request->send(LittleFS, "/html/index.html", String(), false, processor);
  //  request->send(LittleFS, "/html/index.html");
  //});

  //web_server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
  //  request->redirect("/");
  //});


  //AsyncCallbackWebHandler& on(const char* uri, WebRequestMethodComposite method, ArRequestHandlerFunction onRequest, ArUploadHandlerFunction onUpload);
  //web_server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {
  //  if (upload_status) {
  //    request->send(200, "application/json", "{\"upload_status\": 0}"); // maybe return filesize instead so client can verify
  //  }
  //  else {
  //    request->send(500, "application/json", "{\"upload_status\": -1}");
  //  }
  //}, handle_upload);


  web_server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    String message;

    String type = request->getParam("t", true)->value();
    String id = request->getParam("id", true)->value();
    String json = request->getParam("json", true)->value();

    if (id != "" && (type == "am" || type == "pl")) {
      String root = (type == "am") ? AM_ROOT : PL_ROOT;
      String fs_path = form_path(root, id+".json");
      if (save_data(fs_path, json, &message)) {
        gnextup.type = type;
        gnextup.filename = fs_path;
        gfile_list_needs_refresh = true;
      }
    }
    else {
      message = "Invalid type.";
    }

    request->send(200, "application/json", "{\"message\": \""+message+"\"}");
  });


  web_server.on("/load", HTTP_POST, [](AsyncWebServerRequest *request) {
    String message;

    String type = request->getParam("t", true)->value();
    String id = request->getParam("id", true)->value();

    if (id != "" && (type == "am" || type == "pl")) {
      gnextup.type = type;
      gnextup.filename = id;
    }
    else {
      message = "Invalid type.";
    }

    request->send(200, "application/json", "{\"message\": \""+message+"\"}");
  });


  web_server.on("/file_list", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", gfile_list);
  });

  web_server.on("/file_list.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    //request->send(200, "text/plain", gfile_list_json);
    request->send(200, "application/json", gfile_list_json);
  });

  web_server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    for(int i=0; i < params; i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        //DEBUG_PRINTF("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());

        String param_name = p->name();
        // remove leading / and file size from param_name to add only filename to the gdelete_list
        gdelete_list += param_name.substring(1, param_name.indexOf('\t'));
        gdelete_list += "\n";
      }
    }
    request->redirect("/remove.html");
  });

  web_server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      request->send(404);
    }
  });


  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
  web_server.begin();
}


void setup() {

  Serial.begin(115200);

  // instead of using FastLED to manage the POWER used we calculate the max brightness one time below
  //FastLED.setMaxPowerInVoltsAndMilliamps(LED_STRIP_VOLTAGE, LED_STRIP_MILLIAMPS);
  //FastLED.setCorrection(TypicalPixelString);
  FastLED.setCorrection(TypicalSMD5050);  // ??? use this instead
  FastLED.addLeds<WS2812B, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  //FastLED.addLeds<WS2812B, DATA_PIN, RGB>(ledsRGB, getRGBAsize(NUM_LEDS));

  //determine the maximum brightness allowed within POWER limits if 6, 9, 12, and 3 o'clock are a third brightness, and
  //the hour and minute hands are max brightness
  FastLED.clear();
  FastLED.show(); // clear the matrix
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    //leds[i] = CHSV(0, 0, 255); // white
    leds[i] = 0xFFFFFF; // white
    il[i] = TRANSPARENT; // dark green used to represent transparency
  }
  gmax_brightness = calculate_max_brightness_for_power_vmA(leds, NUM_LEDS, 255, LED_STRIP_VOLTAGE, LED_STRIP_MILLIAMPS);
  gmax_brightness = 128; // overriding for testing
  Serial.println(gmax_brightness);
  FastLED.clear();
  FastLED.setBrightness(gmax_brightness);
  FastLED.show();

  //random16_set_seed(analogRead(A0)); // use randomness ??? need to look up which pin for ESP32 ???

  MiskaTonic.set_pattern(NONE);
  MiskaTonic.set_overlay(NO_OVERLAY, false);
  MiskaTonic.set_cb(&noop_cb);

  MiskaTonic.set_autocycle_interval(10000);
  MiskaTonic.set_autocycle_enabled(false);
  MiskaTonic.set_flipflop_interval(6000);
  MiskaTonic.set_flipflop_enabled(false);

  GlowSerum.set_pattern(NONE);
  GlowSerum.set_overlay(NO_OVERLAY, false);
  GlowSerum.set_cb(&noop_cb);

  GlowSerum.set_autocycle_interval(10000);
  GlowSerum.set_autocycle_enabled(false);
  GlowSerum.set_flipflop_interval(6000);
  GlowSerum.set_flipflop_enabled(false);

  FastLED.clear();
  FastLED.show();

  if (!LittleFS.begin()) {
    DEBUG_PRINTLN("LittleFS initialisation failed!");
    while (1) yield(); // cannot proceed without filesystem
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.printf("WiFi Failed!\n");
      return;
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  web_server_initiate();

  handle_file_list(); // refresh file list before starting loop() because refreshing is slow

  //load_matrix_from_file("/files/am/blinky.json");
  
  //load_matrix_from_playlist("/files/pl/tinyrick.json");
  //load_matrix_from_playlist("/files/pl/nyan.json");

  //load_matrix_from_file("/files/am/mushroom_hole_.json");

  load_matrix_from_file("/files/am/bottle_semitransparent.json");


/*
  int8_t i = 16;
  while(true)  {
    Serial.print(i, DEC);
    Serial.print(": ");
    Serial.println(i%16, DEC);
    i--;
    if (i < -16) {
      break;
    }
    delay(1000);
  }
*/


/*
FastLED.clear();
uint8_t alpha = 255;
leds[4] = CRGB::Yellow;
while (true) {

  il[0] = CRGB::Red;
  il[0].a = alpha;
  //leds[0].r = il[0].r;
  //leds[0].g = il[0].g;
  //leds[0].b = il[0].b;
  leds[0] = (CRGB)il[0];


  CRGB bgpixel = leds[4];
  leds[0] = nblend(bgpixel, leds[0], il[0].a);
  FastLED.show();
  delay(1000);
  il[0] = CRGB::Green;
  leds[0].r = il[0].r;
  leds[0].g = il[0].g;
  leds[0].b = il[0].b;
  FastLED.show();
  delay(1000);
  il[0] = CRGB::Blue;
  leds[0].r = il[0].r;
  leds[0].g = il[0].g;
  leds[0].b = il[0].b;
  FastLED.show();
  delay(1000);
  alpha-=16;
  //Serial.println(alpha);
}
*/

}


void loop() {
  static uint32_t pm = 0;
  static uint32_t pm2 = 0;
  static bool refresh_now = true;
  static bool image_has_transparency = true;

  //if ((millis()-pm2) > 2000) {
  //  pm2 = millis();
  //  Serial.print("heap free: ");
  //  Serial.println(esp_get_free_heap_size());
  //}

  // since web_server interrupts we have to queue changes instead of running them directly from the web_server on functions
  // otherwise changes we make could be undo once the interrupt hands back control which could be in the middle of code setting up a different animation
  if (gnextup.filename) {
    if (gnextup.type == "am") {
      gplaylist_enabled = false;
      load_matrix_from_file(gnextup.filename);
    }
    else if (gnextup.type == "pl") {
      load_matrix_from_playlist(gnextup.filename);
    }
    gnextup.type = "";
    gnextup.filename = "";
  }

  if (gplaylist_enabled) {
    refresh_now = load_matrix_from_playlist();
  }

  if(gdemo_enabled) {
    demo(); // cycles through all of the patterns
  }

  if (gpattern_layer_enable && (image_has_transparency || gimage_layer_alpha != 255)) {
    GlowSerum.reanimate(); // pattern layer. changes here are not drawn until they are copied to leds[] when the refresh_block
  }

  static bool dim = 0;
  //EVERY_N_SECONDS(2) { flip(il, dim); dim = !dim;}
  //EVERY_N_SECONDS(1) { move(il, dim, 1); dim = !dim;}
  //EVERY_N_MILLISECONDS(750) { move(il, 1, 1); dim = !dim;}

  static int16_t d = -16;
  //EVERY_N_MILLISECONDS(50) { shift(il, tl, LTR, d, 5); d++; d = (d<=106) ? d : 0;}
  //EVERY_N_MILLISECONDS(50) { uint8_t gap = 5; shift(il, tl, RTL, d, gap); d++; d = d % (16+gap);}
  //EVERY_N_MILLISECONDS(50) {shift(il, tl, RTL, true, 5);}
  //EVERY_N_MILLISECONDS(50) {shift(il, tl, RTL, true, 0);}



  // refresh block
  if((millis()-pm) > 100 || refresh_now) {
    pm = millis();  // worked without this ???
    refresh_now = false;
    image_has_transparency = false;
    gdynamic_hue+=3;
    //gdynamic_hue+=1;
    grandom_hue = random8();

    //sx: + is RTL, - is LTR
    //sy: + is DOWN, - is UP
    //posmove(il, tl, -17, 0, 1, 0, true, 5);


    //sx: + is RTL, - is LTR
    //sy: + is DOWN, - is UP
    //position(il, tl, -5, 0, 1, 0, true, -1);


    // would it work better to write everything to leds[] here first?

    //FYI: !leds[n] does not invert the blue channel, so invert each channel individually
    for (uint16_t i = 0; i < NUM_LEDS; i++) {

      CRGBA ilpixel = il[i];
      uint8_t pixel_alpha = il[i].a;
      if (ilpixel == COLORSUB) {
        ilpixel = CHSV(gdynamic_hue+96, 255, 255); // CHSV overwrites the alpha value
        ilpixel.a = pixel_alpha;
      }

      // transparency is used in two different ways here.
      // 1) if the pixels of the image have an alpha less than 100% (255) then they will be replaced with the background layer.
      // 2) Pixel Art Convertor has an Alpha slider which is separate from the image's alpha channel. This slider controls how much of the
      //    non-transparent parts of the original image are blended with the layers below it.
      // in short 1) local/pixel level effect and 2) global effect
      //

      // nblend destructively overwrites the first argument so need to use temp bgpixel instead of pl[i]
      // otherwise the output would flicker.
      CRGB bgpixel = pl[i];
      uint8_t alpha = scale8(pixel_alpha, gimage_layer_alpha);
      if (alpha != 255) {
        image_has_transparency = true;
        bgpixel.fadeLightBy(64); // fading the background areas make the pixel art stand out
      }

      leds[i] = nblend(bgpixel, (CRGB)ilpixel, alpha);
    }
    // calling FastLED.show(); at the bottom of loop() instead of here gives smoother transitions
  }


  //MiskaTonic.reanimate(); // overlay layer. this writes directly to leds[] so changes are seen immediately.
  FastLED.show(); // what is the best place to call show() ??? call this less frequently ???

  handle_file_list();
  handle_delete_list();
}