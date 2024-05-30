#include <Arduino.h>
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

#include "ArduinoJson-v6.h"

#include "project.h"
#include "ReAnimator.h"

//#include "credentials.h" // set const char *wifi_ssid and const char *wifi_password in include/credentials.h
#include "credentials_private.h"

#define DATA_PIN 16
#define COLOR_ORDER GRB

#define NUM_LAYERS 5

//#define TRANSPARENT (uint32_t)0x424242
#define COLORSUB (uint32_t)0x004200

#define LED_STRIP_VOLTAGE 5
//#define LED_STRIP_MILLIAMPS 1300  // USB power supply.
// the highest current I can measure at full brightness with every pixel white is 2.150 A.
// this measurement does not agree with the power usage calculated by calculate_unscaled_power_mW() nor commonly given methods for estimating LED power usage.
#define LED_STRIP_MILLIAMPS 3750  // 75% (safety margin) of 5000 mA power supply.

#define HOMOGENIZE_BRIGHTNESS true

AsyncWebServer web_server(80);

CRGB leds[NUM_LEDS] = {0}; // output

ReAnimator* layers[NUM_LAYERS];
uint8_t ghost_layers[NUM_LAYERS] = {0};


//uint8_t gmax_brightness = 255;
//const uint8_t gmin_brightness = 2;
uint8_t gdynamic_hue = 0;
uint8_t grandom_hue = 0;
CRGB gdynamic_rgb = 0x000000;
CRGB gdynamic_comp_rgb = 0x000000; // complementary color to the dynamic color
//CRGB gdynamic_comp_rgb = 0xF0AAF0; // complementary color to the dynamic color

uint8_t homogenized_brightness = 255;

//uint8_t gimage_layer_alpha = 255;

bool gplaylist_enabled = false;

struct {
  String type;
  String filename;
} gnextup;

//const char* patterns[] = {"None", "Rainbow", "Solid", "Orbit", "Running Lights", "Juggle", "Sparkle", "Weave", "Checkerboard", "Binary System", "Shooting Star", "Puck-Man", "Cylon", "Demo"};
//const char* accents[] = {"None", "Glitter", "Confetti", "Flicker", "Frozen Decay"};
//const String patterns_json = "{\"patterns\":[\"None\", \"Rainbow\", \"Solid\", \"Orbit\", \"Running Lights\", \"Juggle\", \"Sparkle\", \"Weave\", \"Checkerboard\", \"Binary System\", \"Shooting Star\", \"Puck-Man\", \"Cylon\", \"Demo\"]}";
//const String accents_json = "{\"accents\":[\"None\", \"Glitter\", \"Confetti\", \"Flicker\", \"Frozen Decay\"]}";

//const String patterns_json = "[\"Rainbow\", \"Solid\", \"Orbit\", \"Running Lights\", \"Riffle\", \"Sparkle\", \"Weave\", \"Checkerboard\", \"Binary System\", \"Shooting Star\", \"Puck-Man\", \"Cylon\", \"Demo\"]";
//const String accents_json = "[\"Breathing\", \"Flicker\", \"Frozen Decay\"]";
String patterns_json;
String accents_json;


void homogenize_brightness();
void create_dirs(String path);
void list_files(File dir, String parent);
void handle_file_list(void);
void delete_files(String name, String parent);
void handle_delete_list(void);
String get_root(String type);
String form_path(String root, String id);
bool create_patterns_list();
bool create_accents_list();
bool save_data(String fs_path, String json, String* message = nullptr);
void puck_man_cb(uint8_t event);
bool load_layer(uint8_t lnum, JsonVariant layer_json);
bool load_image_to_layer(uint8_t lnum, String fs_path);
bool load_image_solo(String fs_path);
bool load_composite(String fs_path);
bool load_file(String fs_path);
bool load_from_playlist(String id = "");
void web_server_initiate(void);



// uses custom values for LED power usage.
// homogenize_brightness_custom() is a combination of calculate_max_brightness_for_power_mW() and calculate_unscaled_power_mW()
// found in FastLED's power_mgt.cpp. The code was borrowed from these functions in order to redefine the power used per LED color.
// My measurements showed the defaults to be much higher than my LEDs which resulted in the calculated brightness being much lower
// than it could be.
void homogenize_brightness_custom() {
  //static const uint8_t red_mW   = 16 * 5; ///< 16mA @ 5v = 80mW
  //static const uint8_t green_mW = 11 * 5; ///< 11mA @ 5v = 55mW
  //static const uint8_t blue_mW  = 15 * 5; ///< 15mA @ 5v = 75mW
  //static const uint8_t dark_mW  =  1 * 5; ///<  1mA @ 5v =  5mW

  static const uint8_t red_mW   = 11 * 5; ///< 11mA @ 5v = 55mW
  static const uint8_t green_mW = 11 * 5; ///< 11mA @ 5v = 55mW
  static const uint8_t blue_mW  = 11 * 5; ///< 11mA @ 5v = 55mW
  static const uint8_t dark_mW  =  1 * 5; ///<  1mA @ 5v =  5mW

  uint32_t max_power_mW = LED_STRIP_VOLTAGE * LED_STRIP_MILLIAMPS;

  uint32_t red32 = 0, green32 = 0, blue32 = 0;
  const CRGB* firstled = &(leds[0]);
  uint8_t* p = (uint8_t*)(firstled);

  uint16_t count = NUM_LEDS;

  while (count) {
    red32   += *p++;
    green32 += *p++;
    blue32  += *p++;
    --count;
  }

  red32   *= red_mW;
  green32 *= green_mW;
  blue32  *= blue_mW;

  red32   >>= 8; // ideally this would be divide by 255 since max value of a color channel is 255, but >> 8 (i.e. divide by 256) is faster.
  green32 >>= 8;
  blue32  >>= 8;

  //uint32_t total_mW = red32 + green32 + blue32 + (dark_mW * NUM_LEDS);
  uint32_t total_dark_mW = 780; // measured.
  uint32_t total_mW = red32 + green32 + blue32 + total_dark_mW;

	uint32_t requested_power_mW = ((uint32_t)total_mW * homogenized_brightness) / 256;


	if (requested_power_mW > max_power_mW) { 
    homogenized_brightness = (uint32_t)((uint8_t)(homogenized_brightness) * (uint32_t)(max_power_mW)) / ((uint32_t)(requested_power_mW));
	}
}

//uses builtin values for LED power usage
void homogenize_brightness_builtin() {
    uint8_t max_brightness = calculate_max_brightness_for_power_vmA(leds, NUM_LEDS, homogenized_brightness, LED_STRIP_VOLTAGE, LED_STRIP_MILLIAMPS);
    if (max_brightness < homogenized_brightness) {
        homogenized_brightness = max_brightness;
    }
}


// When FastLED's power management functions are used FastLED dynamically adjusts the brightness level to be as high as possible while
// keeping the power draw near the specified level. This can lead to the brightness level of an animation noticeably increasing when
// fewer LEDs are lit and the brightness noticeably dipping when more LEDs are lit or their colors change.
// homogenize_brightness() learns the lowest brightness level of all the animations and uses it across every animation to keep a consistent
// brightness level. This will lead to dimmer animations and power usage that is almost always a good bit lower than what the FastLED power
// management function was set to aim for. Set the #define for HOMOGENIZE_BRIGHTNESS to false to disable this feature.
//
// Besides keeping the brightness consistent, homogenize_brightness() will also find the maximum safe brightness automatically.
// homogenize_brightness() is run before any animation is shown, so it sees power required.
// This is preferable to calculating the max brightness in setup() by setting the LEDs to a guess at what pattern of colors might need the
// maximum amount of power. The safest guess is all white LEDs; however none of your animations may ever require that much power so using
// all white to determine the max brightness is giving up brightness that could be safely used.
void homogenize_brightness() {
    homogenize_brightness_builtin();
    //homogenize_brightness_custom();
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


String get_root(String type) {
  if (type == "im") {
    return IM_ROOT;
  }
  if (type == "pl") {
    return PL_ROOT;
  }
  if (type == "cm") {
    return CM_ROOT;
  }
  return "";
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


bool save_data(String fs_path, String json, String* message) {
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


bool create_patterns_list() {
  static Pattern pattern_id = static_cast<Pattern>(0);
  String pattern_name;
  bool match = false;
  static bool first = true;
  bool finished = false;
  Serial.println(pattern_id);
  switch(pattern_id) {
    default:
        if (pattern_id > 49) {
          pattern_id = static_cast<Pattern>(0);
          finished = true;
        }
        break;
    // Note: it does not make sense to present NO_PATTERN as an option in the frontend
    case DYNAMIC_RAINBOW:
        pattern_name = "Rainbow";
        match = true;
        break;
    case SOLID:
        pattern_name = "Solid";
        match = true;
        break;
    case ORBIT:
        pattern_name = "Orbit";
        match = true;
        break;
    case RUNNING_LIGHTS:
        pattern_name = "Running Lights";
        match = true;
        break;
    case RIFFLE:
        pattern_name = "Riffle";
        match = true;
        break;
    case SPARKLE:
        pattern_name = "Sparkle";
        match = true;
        break;
    case WEAVE:
        pattern_name = "Weave";
        match = true;
        break;
    case CHECKERBOARD:
        pattern_name = "Checkerboard";
        match = true;
        break;
    case BINARY_SYSTEM:
        pattern_name = "Binary System";
        match = true;
        break;
    case SHOOTING_STAR:
        pattern_name = "Shooting Star";
        match = true;
        break;
    case PUCK_MAN:
        pattern_name = "Puck-Man";
        match = true;
        break;
    case CYLON:
        pattern_name = "Cylon";
        match = true;
        break;
  }
  if (match) {
    if (!first) {
      patterns_json += ",";
    }
    patterns_json += "{\"name\":\"";
    patterns_json += pattern_name;
    patterns_json += "\",\"id\":";
    patterns_json += pattern_id;
    patterns_json += "}";
    first = false;
  }
  if (!finished) {
    pattern_id = static_cast<Pattern>(pattern_id+1);
  }
  return finished;
}


bool create_accents_list() {
  static Overlay accent_id = static_cast<Overlay>(0);
  String accent_name;
  bool match = false;
  static bool first = true;
  bool finished = false;
  Serial.println(accent_id);
  switch(accent_id) {
    default:
        if (accent_id > 49) {
          accent_id = static_cast<Overlay>(0);
          finished = true;
        }
        break;
    // Since accents are an optional, secondary effect it makes sense to present an option to have no accent in the frontend
    case NO_OVERLAY:
        accent_name = "None";
        match = true;
        break;
    case BREATHING:
        accent_name = "Rainbow";
        match = true;
        break;
    case FLICKER:
        accent_name = "Solid";
        match = true;
        break;
    case FROZEN_DECAY:
        accent_name = "Orbit";
        match = true;
        break;
  }
  if (match) {
    if (!first) {
      accents_json += ",";
    }
    accents_json += "{\"name\":\"";
    accents_json += accent_name;
    accents_json += "\",\"id\":";
    accents_json += accent_id;
    accents_json += "}";
    first = false;
  }
  if (!finished) {
    accent_id = static_cast<Overlay>(accent_id+1);
  }
  return finished;
}


void puck_man_cb(uint8_t event) {
  static bool one_shot = false;
  switch (event) {
    case 0:
      one_shot = false;
      for (uint8_t i = 0; i < NUM_LAYERS; i++) {
        if (ghost_layers[i] == 1) {
          if (layers[i])
          load_image_to_layer(i, "/files/im/blinky.json");
        }
        if (ghost_layers[i] == 2) {
          load_image_to_layer(i, "/files/im/pinky.json");
        }
        if (ghost_layers[i] == 3) {
          load_image_to_layer(i, "/files/im/inky.json");
        }
        if (ghost_layers[i] == 4) {
          load_image_to_layer(i, "/files/im/clyde.json");
        }
      }
      break;
    case 1:
      if (!one_shot) {
        one_shot = true;
        for (uint8_t i = 0; i < NUM_LAYERS; i++) {
          if (ghost_layers[i]) {
            load_image_to_layer(i, "/files/im/blue_ghost.json");
          }
        }
      }
      break;
    case 2:
      for (uint8_t i = 0; i < NUM_LAYERS; i++) {
        if (ghost_layers[i]) {
          if (layers[i] != nullptr) {
            layers[i]->clear();
          }
        }
      }
      break;
    default:
      break;
  }
}


bool load_layer(uint8_t lnum, JsonVariant layer_json) {
  //if (layer_json[F("t")] == "e" || layer_json[F("t")].isNull() || layer_json[F("id")].isNull() || (layer_json[F("t")] == "t" && layer_json[F("w")].isNull()) ) {
  if (layer_json[F("t")] == "e" || layer_json[F("t")].isNull() || layer_json[F("id")].isNull()) {
    if (layers[lnum] != nullptr) {
      delete layers[lnum];
      layers[lnum] = nullptr;
    }
    return true;
  }

  if (layers[lnum] == nullptr) {
    layers[lnum] = new ReAnimator();
  }

  // sane defaults in case data is missing.
  // if this data is missing json is perhaps it is better to not show the layer at all.
  uint8_t id2 = 0;
  uint8_t ct = 0; // dynamic color
  std::string c = "0xFFFF00"; // yellow if the fixed color
  uint8_t m = 0; // no movement


  if (!layer_json[F("id2")].isNull()) {
    id2 = layer_json[F("id2")];
  }

  // color is not yet used for images. may be implemented in the future to change color palettes.
  if (!layer_json[F("ct")].isNull()) {
    ct = layer_json[F("ct")];
  }
  if (ct == 0) {          
    layers[lnum]->set_color(&gdynamic_rgb);
  }
  else if (ct == 1) {          
    layers[lnum]->set_color(&gdynamic_comp_rgb);
  }
  else {
    if (!layer_json[F("c")].isNull()) {
      c = layer_json[F("c")].as<std::string>();
    }
    uint32_t fc = std::stoul(c, nullptr, 16);
    layers[lnum]->set_color(fc);
  }


  if (!layer_json[F("m")].isNull()) {
    m = layer_json[F("m")];
  }

  if (layer_json[F("t")] == "i") {
    String id = layer_json[F("id")];
    ghost_layers[lnum] = 0;
    if (id.endsWith("/blinky.json")) {
      ghost_layers[lnum] = 1;
    }
    if (id.endsWith("/pinky.json")) {
      ghost_layers[lnum] = 2;
    }
    if (id.endsWith("/inky.json")) {
      ghost_layers[lnum] = 3;
    }
    if (id.endsWith("/clyde.json")) {
      ghost_layers[lnum] = 4;
    }
    layers[lnum]->setup(Image_t, -2);
    layers[lnum]->set_image(id);
    layers[lnum]->set_overlay(static_cast<Overlay>(id2), true);
    layers[lnum]->set_heading(m);
  }
  else if (layer_json[F("t")] == "p") {
    uint8_t id = layer_json[F("id")];
    layers[lnum]->setup(Pattern_t, id);
    layers[lnum]->set_cb(&puck_man_cb);
    layers[lnum]->set_pattern(static_cast<Pattern>(id));
    layers[lnum]->set_overlay(static_cast<Overlay>(id2), true);
    layers[lnum]->set_heading(m);
  }
  //else if (layer_json[F("t")] == "a") {
  //  uint8_t id = layer_json[F("id")];
  //  layers[lnum]->setup(Accent_t, id);
  //  layers[lnum]->set_overlay(static_cast<Overlay>(id), true);
  //  layers[lnum]->set_heading(m);
  //}
  else if (layer_json[F("t")] == "t") {
    layers[lnum]->setup(Text_t, -1);
    layers[lnum]->set_text(layer_json[F("w")]);
    layers[lnum]->set_overlay(static_cast<Overlay>(id2), true);
    // direction is disabled for text in the frontend. setting to default of 0.
    // if it is not set back to 0 on a layer that previous had movement the text
    // will move.
    layers[lnum]->set_heading(m);
  }
  else if (layer_json[F("t")] == "n") {
    uint8_t id = layer_json[F("id")];
    layers[lnum]->setup(Info_t, id);
    layers[lnum]->set_info(static_cast<Info>(id));
    layers[lnum]->set_overlay(static_cast<Overlay>(id2), true);
    layers[lnum]->set_heading(m);
  }
  return true;
}

// this checks the layer exists before loading an image to it.
// the idea is that the layer exists and we want to preserve all
// its other attributes but replace its image.
// this helps streamline code from having the same repeative layer
// existence check.
bool load_image_to_layer(uint8_t lnum, String fs_path) {
  bool retval = false;
  if (layers[lnum] != nullptr) {
    retval = layers[lnum]->set_image(fs_path);
  }
  return retval;
}


// this loads an image by itself (no other layers) to layer 0.
bool load_image_solo(String fs_path) {
  bool retval = false;
  for (uint8_t i = 0; i < NUM_LAYERS; i++) {
    if (layers[i] != nullptr) {
      delete layers[i];
      layers[i] = nullptr;
    }
  }

  if (layers[0] == nullptr) {
    layers[0] = new ReAnimator();
    layers[0]->set_color(CRGB::White);
    //layers[0]->setup(Image_t, -2);
    retval = layers[0]->set_image(fs_path);
    layers[0]->set_heading(0);
  }

  // can also load image to layer like this to be more consistent with load_composite()
  //StaticJsonDocument<256> doc;
  //JsonVariant layer_json = doc.to<JsonVariant>();
  //layer_json["t"] = "i";
  //layer_json["id"] = fs_path;
  //layer_json["ct"] = 0;
  //layer_json["m"] = 0;
  //retval = load_layer(0, layer_json);

  return retval;
}


bool load_composite(String fs_path) {
  bool retval = false;
  File file = LittleFS.open(fs_path, "r");
  
  if(!file){
    return false;
  }

  if (file.available()) {
    DynamicJsonDocument doc(768);
    String json = file.readString();
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
      DEBUG_PRINT("deserializeJson() failed: ");
      DEBUG_PRINTLN(error.c_str());
      return false;
    }

    JsonObject object = doc.as<JsonObject>();
    JsonArray arr = object[F("l")];
    if (!arr.isNull() && arr.size() > 0) {
      for (uint8_t i = 0; i < arr.size(); i++) {
        if(arr[i].is<JsonVariant>()) {
          retval = load_layer(i, arr[i]);
        }
      }
    }
  }

  return retval;
}


bool load_file(String fs_path) {
  bool retval = false;
  if (fs_path.startsWith(CM_ROOT)) {
    retval = load_composite(fs_path);
  }
  else if (fs_path.startsWith(IM_ROOT)) {
    retval = load_image_solo(fs_path);
  }
  return true;
}


bool load_from_playlist(String id) {
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

    File file = LittleFS.open(_id, "r");
    if (file && file.available()) {
      String json = file.readString();
      file.close();

      DynamicJsonDocument doc(2048); //DynamicJsonDocument (vs StaticJsonDocument) recommended for documents larger than 1KB

      DeserializationError error = deserializeJson(doc, json);
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        gplaylist_enabled = false;
        refresh_needed = false;
        return refresh_needed;
      }

      JsonObject object = doc.as<JsonObject>();
      JsonArray arr = object[F("am")];
      if (!arr.isNull() && arr.size() > 0) {
        if(arr[i].is<JsonVariant>()) {
          JsonVariant am = arr[i];
          //String msg = "load_from_playlist(): playlist disabled.";
          String path = am[F("id")];
          if(load_file(path)) {
            if(am[F("d")].is<JsonInteger>()) {
              am_duration = am[F("d")];
            }
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
      }
    }
  }
  return refresh_needed;
}


void web_server_initiate() {

  web_server.serveStatic("/", LittleFS, "/").setDefaultFile("html/index.html");

  web_server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/index.html");
  });

  web_server.on("/converter.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/converter.html");
  });

  web_server.on("/compositor.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/compositor.html");
  });

  web_server.on("/playlist_maker.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/playlist_maker.html");
  });

  web_server.on("/play.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/play.html");
  });

  web_server.on("/file_manager.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/file_manager.html");
  });


  // perhaps use the processor to replace references to IM_ROOT and PL_ROOT in html files
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
    int rc = 400;
    String message;

    String type = request->getParam("t", true)->value();
    String id = request->getParam("id", true)->value();
    String json = request->getParam("json", true)->value();
    String root = get_root(type);

    if (id != "" && root != "") {
      String fs_path = form_path(root, id+".json");
      if (save_data(fs_path, json, &message)) {
        gnextup.type = type;
        gnextup.filename = fs_path;
        gfile_list_needs_refresh = true;
        rc = 200;
      }
    }
    else {
      message = "Invalid type.";
    }

    request->send(rc, "application/json", "{\"message\": \""+message+"\"}");
  });


  web_server.on("/load", HTTP_POST, [](AsyncWebServerRequest *request) {
    int rc = 400;
    String message;

    String type = request->getParam("t", true)->value();
    String id = request->getParam("id", true)->value();

    if (id != "" && (type == "im" || type == "cm" || type == "pl")) {
      gnextup.type = type;
      gnextup.filename = id;
      message = gnextup.filename + " queued.";
      rc = 200;
    }
    else {
      message = "Invalid type.";
    }

    request->send(rc, "application/json", "{\"message\": \""+message+"\"}");
  });

  // memory hog?
  web_server.on("/options.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    String options_json = "{\"files\":"+gfile_list_json + ",\"patterns\":["+patterns_json + "],\"accents\":["+accents_json + "]}"; 
    request->send(200, "application/json", options_json);
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

  for (uint8_t i = 0; i < NUM_LAYERS; i++) {
    layers[i] = nullptr;
  }

  Serial.begin(115200);

  // setMaxPowerInVoltsAndMilliamps() should not be used if homogenize_brightness_custom() is used
  // since setMaxPowerInVoltsAndMilliamps() uses the builtin LED power usage constants 
  // homogenize_brightness_custom() was created to avoid.
  FastLED.setMaxPowerInVoltsAndMilliamps(LED_STRIP_VOLTAGE, LED_STRIP_MILLIAMPS);
  FastLED.setCorrection(TypicalSMD5050);
  FastLED.addLeds<WS2812B, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

  FastLED.clear();
  FastLED.show(); // clear the matrix on startup

  // for taking current measurements at different brightness levels
  // every LED white is the maximum amout of power that can be used
  //for (uint16_t i = 0; i < NUM_LEDS; i++) {
  //  leds[i] = 0xFFFFFF; // white
  //}
  //FastLED.setBrightness(255);
  //homogenize_brightness();
  //FastLED.setBrightness(homogenized_brightness);
  //Serial.println(homogenized_brightness); // builtin: 87, custom: 112
  //FastLED.show();
  //delay(6000);

  homogenize_brightness();
  FastLED.setBrightness(homogenized_brightness);
  Serial.println(homogenized_brightness);

  FastLED.clear();
  FastLED.setBrightness(homogenized_brightness);

  //random16_set_seed(analogRead(A0)); // use randomness ??? need to look up which pin for ESP32 ???

  if (!LittleFS.begin()) {
    DEBUG_PRINTLN("LittleFS initialisation failed!");
    while (1) yield(); // cannot proceed without filesystem
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
      Serial.println("WiFi Failed!");
      return;
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  web_server_initiate();

  handle_file_list(); // refresh file list before starting loop() because refreshing is slow

  while(!create_patterns_list());
  while(!create_accents_list());

  // initialzie dynamic colors because otherwise they won't be set until after layer.refresh() has been called which can lead to partially black text
  gdynamic_rgb = CHSV(gdynamic_hue, 255, 255);
  gdynamic_comp_rgb = CRGB::White - gdynamic_rgb;

  load_file("/files/cm/mycomp.json");
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

  // since web_server interrupts we have to queue changes instead of running them directly from web_server's on functions
  // otherwise changes we make could be undo once the interrupt hands back control which could be in the middle of code setting up a different animation
  if (gnextup.filename) {
    if (gnextup.type == "pl") {
      load_from_playlist(gnextup.filename);
    }
    else if (gnextup.type == "cm") {
      gplaylist_enabled = false;
      load_file(gnextup.filename);
    }
    else if (gnextup.type == "im") {
      gplaylist_enabled = false;
      load_file(gnextup.filename);
    }
    gnextup.type = "";
    gnextup.filename = "";
  }

  if (gplaylist_enabled) {
    refresh_now = load_from_playlist();
  }

  // draw layers. changes in layers are not displayed until they are copied to leds[] in the blend block
  for (uint8_t i = 0; i < NUM_LAYERS; i++) {
    if (layers[i] != nullptr) {
      layers[i]->reanimate();
    }
  }

  // blend block
  if ((millis()-pm) > 100 || refresh_now) {
    pm = millis();
    refresh_now = false;
    image_has_transparency = false;
    gdynamic_hue+=3;
    gdynamic_rgb = CHSV(gdynamic_hue, 255, 255);
    gdynamic_comp_rgb = CRGB::White - gdynamic_rgb;
    grandom_hue = random8();


    FastLED.clear(); // use clear instead of tracking bg layer.
    CRGBA pixel;
    //bool is_bg_layer = true;
    for (uint8_t i = 0; i < NUM_LAYERS; i++) {
      if (layers[i] != nullptr) {
        for (uint16_t j = 0; j < NUM_LEDS; j++) {
          // transparency is used in two different ways here.
          // 1) if the pixels of the image have an alpha less than 100% (255) then they will be replaced with the background layer.
          // 2) !!NOT IMPLEMENTED!! Layer composer has an Alpha slider which is separate from the image's alpha channel. This slider controls how much of the
          //    non-transparent parts of the original image are blended with the layers below it.
          // in short 1) local/pixel level effect and 2) global effect
          //

          image_has_transparency = true; // for testing
          pixel = layers[i]->get_pixel(j);


          // color substitution experiment. hard to get exact color in the Pixel Art Creator
          //uint8_t pixel_alpha = pixel.a;
          //if (pixel == COLORSUB) {
          //  pixel = CHSV(gdynamic_hue+96, 255, 255); // CHSV overwrites the alpha value
          //  pixel.a = pixel_alpha;
          //}

          // treat the first non-empty layer we touch as the background layer and give it zero transparency to completely overwrite the previous frame.
          uint8_t alpha = pixel.a;
          //if (is_bg_layer) {
          //  alpha = 255;
          //}

          CRGB bgpixel = leds[j];
          // before implementing pixel level transparency previously used a global alpha that is not currently implemented
          //alpha = scale8(alpha, gimage_layer_alpha);
          if (alpha != 255) {
            image_has_transparency = true;
            //bgpixel.fadeLightBy(64); // fading the background areas make the pixel art stand out
          }
          leds[j] = nblend(bgpixel, (CRGB)pixel, alpha);

          //leds[j] = (CRGB)pixel;
        }
/*
        Serial.print("layer ");
        Serial.print(i);
        Serial.print(" :: ");
        Serial.print("red: ");
        Serial.print(pixel.r);
        Serial.print("| green: ");
        Serial.print(pixel.g);
        Serial.print("| blue: ");
        Serial.print(pixel.b);
        Serial.print("| alpha: ");
        Serial.println(pixel.a);
        Serial.println((uint32_t)pixel, HEX);
*/
        //is_bg_layer = false;
      }
    }

#if HOMOGENIZE_BRIGHTNESS
    homogenize_brightness();
#endif
    // safety measure while testing
    //if (homogenized_brightness > 128) {
    //  Serial.print("homogenized_brightness > 128: ");
    //  Serial.println(homogenized_brightness);
    //  homogenized_brightness = 128;
    //}
    FastLED.setBrightness(homogenized_brightness);
  }

  FastLED.show(); // what is the best place to call show() ??? call this less frequently ???

  handle_file_list();
  handle_delete_list();
}