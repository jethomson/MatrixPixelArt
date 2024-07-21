#include <Arduino.h>
#include <FS.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
//#include <SPIFFSEditor.h>
#include <LittleFS.h>
//#include <SPI.h>
#include <Preferences.h>

#include <FastLED.h>
#include "FastLED_RGBA.h"

#include "ArduinoJson-v6.h"
#include <StreamUtils.h>

#include <unordered_set>

#include "project.h"
#include "ReAnimator.h"

#define WIFI_CONNECT_TIMEOUT 10000 // milliseconds
#define SOFT_AP_SSID "PixelArt"
#define MDNS_HOSTNAME "pixelart"

// these values are used if the number of rows and columns of the LED matrix are not provided
// on the configuration page of the frontend
#define DEFAULT_NUM_ROWS 16
#define DEFAULT_NUM_COLS 16

#define DATA_PIN 16
#define COLOR_ORDER GRB

#define LED_STRIP_VOLTAGE 5
//#define LED_STRIP_MILLIAMPS 1300  // USB power supply.
// the highest current I can measure at full brightness with every pixel white is 2.150 A.
// this measurement does not agree with the power usage calculated by calculate_unscaled_power_mW() nor commonly given methods for estimating LED power usage.
#define LED_STRIP_MILLIAMPS 3750  // 75% (safety margin) of 5000 mA power supply.

#define HOMOGENIZE_BRIGHTNESS true

AsyncWebServer web_server(80);

DNSServer dnsServer;

Preferences preferences;

bool grestart_needed = false;
bool gdns_up = false;

IPAddress IP;
String mdns_host;

uint8_t NUM_ROWS = 0;
uint8_t NUM_COLS = 0;
uint16_t NUM_LEDS = 0;
CRGB* leds; // output


ReAnimator* layers[NUM_LAYERS];
uint8_t ghost_layers[NUM_LAYERS] = {0};

//uint8_t gmax_brightness = 255;
//const uint8_t gmin_brightness = 2;
uint8_t gdynamic_hue = 0;
uint8_t grandom_hue = 0;
CRGB gdynamic_rgb = 0x000000;
CRGB gdynamic_comp_rgb = 0x000000; // complementary color to the dynamic color

uint8_t homogenized_brightness = 255;

//uint8_t gimage_layer_alpha = 255;

bool gplaylist_enabled = false;

struct {
  String type;
  String id;
} gnextup;

String patterns_json;
String accents_json;

// we have a JsonArray that needs to persist after load_from_playlist() exits,
// therefore the document the JsonArray references must persist between function calls.
//DynamicJsonDocument gpldoc(2048); //DynamicJsonDocument is recommended for documents larger than 1KB
StaticJsonDocument<2048> gpldoc; // about 5 ms faster than DynamicJsonDocument. faster here compared to declaring as static inside load_from_playlist().

void homogenize_brightness_custom(void);
void homogenize_brightness_builtin(void);
void homogenize_brightness(void);
void create_dirs(String path);
void list_files(File dir, String parent);
void handle_file_list(void);
void delete_files(String name, String parent);
void handle_delete_list(void);
bool create_patterns_list(void);
bool create_accents_list(void);
bool save_data(String fs_path, String json, String* message = nullptr);
void puck_man_cb(uint8_t event);
bool load_layer(uint8_t lnum, JsonVariant layer_json);
bool load_image_to_layer(uint8_t lnum, String id);
bool load_image_solo(String id);
bool load_composite(String fs_path);
bool load_file(String type, String id);
bool load_from_playlist(String id = "");

bool attempt_connect(void);
String get_ip(void);
String get_mdns_addr(void);
void wifi_AP(void);
bool wifi_connect(void);
void web_server_station_setup(void);
void web_server_ap_setup(void);
void web_server_initiate(void);
void show(void);


// uses custom values for LED power usage.
// homogenize_brightness_custom() is a combination of calculate_max_brightness_for_power_mW() and calculate_unscaled_power_mW()
// found in FastLED's power_mgt.cpp. The code was borrowed from these functions in order to redefine the power used per LED color.
// My measurements showed the defaults to be much higher than my LEDs which resulted in the calculated brightness being much lower
// than it could be.
void homogenize_brightness_custom(void) {
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
void homogenize_brightness_builtin(void) {
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
void homogenize_brightness(void) {
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
std::unordered_set<std::string> image_list_tmp;
std::unordered_set<std::string> image_list;
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

  //gfile_list_json_tmp += "{\"t\":\"dir\",\"id\":\"";
  //gfile_list_json_tmp += path;
  //gfile_list_json_tmp += "\"},";

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

      String type = dir.name();
      String id = entry.name();
      id.remove(id.length()-5); // remove .json extension
      if (type == "im" || type == "cm") {
        gfile_list_json_tmp += "{\"t\":\"";
        gfile_list_json_tmp += type;
        gfile_list_json_tmp += "\",";
        gfile_list_json_tmp += "\"id\":\"";
        gfile_list_json_tmp += id;
        gfile_list_json_tmp += "\"},";
        image_list_tmp.insert(id.c_str());
      }
      else if (type == "pl") {
        gfile_list_json_tmp += "{\"t\":\"";
        gfile_list_json_tmp += type;
        gfile_list_json_tmp += "\",\"p\":\"";
        gfile_list_json_tmp += path; // the frontend only needs paths for playlists
        gfile_list_json_tmp += "/";
        gfile_list_json_tmp += entry.name();
        gfile_list_json_tmp += "\",";
        gfile_list_json_tmp += "\"id\":\"";
        gfile_list_json_tmp += id;
        gfile_list_json_tmp += "\"},";
      }

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
    gfile_list_json_tmp = "";
    image_list_tmp.clear();
    
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

    image_list = image_list_tmp;
    image_list_tmp.clear();
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

// creating the patterns list procedurally rather than hardcoding it allows for rearranging and adding to the patterns that appear
// in the frontend more easily. this function creates the list based on what is in the enum Pattern, so changes to that enum
// are reflected here. assigning a new number to the pattern will change where the pattern falls in the list sent to the frontend.
// setting the pattern to a number of 50 or greater will remove it from the list sent to the frontend.
// if a new pattern is created a new case pattern_name will still need to be set here.
bool create_patterns_list(void) {
  static Pattern pattern_id = static_cast<Pattern>(0);
  String pattern_name;
  bool match = false;
  static bool first = true;
  bool finished = false;
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
    case PENDULUM:
        pattern_name = "Pendulum";
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
    case FUNKY:
        pattern_name = "Funky";
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


bool create_accents_list(void) {
  static Overlay accent_id = static_cast<Overlay>(0);
  String accent_name;
  bool match = false;
  static bool first = true;
  bool finished = false;
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
        accent_name = "Breathing";
        match = true;
        break;
    case FLICKER:
        accent_name = "Flicker";
        match = true;
        break;
    case FROZEN_DECAY:
        accent_name = "Frozen Decay";
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
          load_image_to_layer(i, "blinky");
        }
        if (ghost_layers[i] == 2) {
          load_image_to_layer(i, "pinky");
        }
        if (ghost_layers[i] == 3) {
          load_image_to_layer(i, "inky");
        }
        if (ghost_layers[i] == 4) {
          load_image_to_layer(i, "clyde");
        }
      }
      break;
    case 1:
      if (!one_shot) {
        one_shot = true;
        for (uint8_t i = 0; i < NUM_LAYERS; i++) {
          if (ghost_layers[i]) {
            load_image_to_layer(i, "blue_ghost");
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
  if (layer_json[F("t")] == "e" || layer_json[F("t")].isNull() || layer_json[F("id")].isNull() || (layer_json[F("t")] == "t" && layer_json[F("w")].isNull()) ) {
    if (layers[lnum] != nullptr) {
      delete layers[lnum];
      layers[lnum] = nullptr;
    }
    return true;
  }

  if (layers[lnum] == nullptr) {
    layers[lnum] = new ReAnimator(NUM_ROWS, NUM_COLS);
  }

  // sane defaults in case data is missing.
  // if this data is missing json is perhaps it is better to not show the layer at all.
  uint8_t accent_id = 0;
  uint8_t color_type = 0; // dynamic color
  std::string color = "0xFFFF00"; // yellow as a warning
  uint8_t movement = 0; // no movement

  if (!layer_json[F("a")].isNull()) {
    accent_id = layer_json[F("a")];
  }

  // color is not yet used for images. may be implemented in the future to change color palettes.
  if (!layer_json[F("ct")].isNull()) {
    color_type = layer_json[F("ct")];
  }
  if (color_type == 0) {          
    layers[lnum]->set_color(&gdynamic_rgb);
  }
  else if (color_type == 1) {          
    layers[lnum]->set_color(&gdynamic_comp_rgb);
  }
  else {
    if (!layer_json[F("c")].isNull()) {
      color = layer_json[F("c")].as<std::string>();
    }
    uint32_t fixed_color = std::stoul(color, nullptr, 16);
    layers[lnum]->set_color(fixed_color);
  }

  if (!layer_json[F("m")].isNull()) {
    movement = layer_json[F("m")];
  }

  ghost_layers[lnum] = 0;
  if (layer_json[F("t")] == "im") {
    String id = layer_json[F("id")];
    if (id == "blinky") {
      ghost_layers[lnum] = 1;
    }
    if (id == "pinky") {
      ghost_layers[lnum] = 2;
    }
    if (id == "inky") {
      ghost_layers[lnum] = 3;
    }
    if (id == "clyde") {
      ghost_layers[lnum] = 4;
    }
    layers[lnum]->setup(Image_t, -2);
    load_image_to_layer(lnum, id);
    layers[lnum]->set_overlay(static_cast<Overlay>(accent_id), true);
    layers[lnum]->set_heading(movement);
  }
  else if (layer_json[F("t")] == "p") {
    uint8_t id = layer_json[F("id")];
    layers[lnum]->setup(Pattern_t, id);
    layers[lnum]->set_cb(&puck_man_cb);
    layers[lnum]->set_pattern(static_cast<Pattern>(id));
    layers[lnum]->set_overlay(static_cast<Overlay>(accent_id), true);
    layers[lnum]->set_heading(movement);
  }
  else if (layer_json[F("t")] == "t") {
    layers[lnum]->setup(Text_t, -1);
    layers[lnum]->set_text(layer_json[F("w")]);
    layers[lnum]->set_overlay(static_cast<Overlay>(accent_id), true);
    // direction is disabled for text in the frontend. setting to default of 0.
    // if it is not set back to 0 on a layer that previously had movement the text
    // will move.
    layers[lnum]->set_heading(movement);
  }
  else if (layer_json[F("t")] == "n") {
    uint8_t id = layer_json[F("id")];
    layers[lnum]->setup(Info_t, id);
    layers[lnum]->set_info(static_cast<Info>(id));
    layers[lnum]->set_overlay(static_cast<Overlay>(accent_id), true);
    layers[lnum]->set_heading(movement);
  }
  return true;
}

// this checks the layer and image exists before loading an image to it.
// the idea is that the layer exists and we want to preserve all
// its other attributes but replace its image.
// this helps streamline code from having the same repeative layer and image
// existence checks.
bool load_image_to_layer(uint8_t lnum, String id) {
  // the set image_list is used instead of LittleFS.exists() because exists() is much slower
  if ( layers[lnum] != nullptr && image_list.find(id.c_str()) != image_list.end() ) {
    layers[lnum]->set_image(id);
    // since the image is loaded asynchronously using another core to prevent lag
    // waiting to determine if the image was loaded successfully would defeat the purpose.
    // so the best we can do is check if the image exists for indicating success.
    // this file existence check helps playlist handling perform better because it means
    // non-existent images can be skipped.
    return true;
  }
  return false;
}


// this loads an image by itself (no other layers) to layer 0.
bool load_image_solo(String id) {
  bool retval = false;
  for (uint8_t i = 0; i < NUM_LAYERS; i++) {
    if (layers[i] != nullptr) {
      delete layers[i];
      layers[i] = nullptr;
    }
  }

  if (layers[0] == nullptr) {
    layers[0] = new ReAnimator(NUM_ROWS, NUM_COLS);
    layers[0]->setup(Image_t, -2);
    layers[0]->set_color(&gdynamic_rgb);
    retval = load_image_to_layer(0, id);
    layers[0]->set_heading(0);
  }

  return retval;
}


bool load_composite(String id) {
  bool retval = false;
  String fs_path = form_path(F("cm"), id);
  File file = LittleFS.open(fs_path, "r");
  
  if(!file){
    return false;
  }

  if (file.available()) {
    // 360 ms on average
    // 333 ms on average
    // 346 ms on average

    // 214 ms on average total time between show(), global StaticJSONDoc
    //String json = file.readString();
    //DeserializationError error = deserializeJson(gcmdoc, json);
    //file.close();

    // 241 ms on average total time between show(), global DynamicJSONDoc
    // 212 ms on average total time between show(), global StaticJSONDoc
    //DeserializationError error = deserializeJson(gcmdoc, file);
    //file.close();

    // 213 ms on average total time between show(), global DynamicJSONDoc
    // 209 ms on average total time between show(), global StaticJSONDoc
    // 189 ms on average total time between show(), local DynamicJSONDoc
    // 186 ms on average total time between show(), local StaticJSONDoc
    //DynamicJsonDocument gcmdoc(768);
    StaticJsonDocument<768> gcmdoc;
    ReadBufferingStream bufferedFile(file, 64);
    DeserializationError error = deserializeJson(gcmdoc, bufferedFile);
    file.close();

    if (error) {
      DEBUG_PRINT("deserializeJson() failed: ");
      DEBUG_PRINTLN(error.c_str());
      return false;
    }

    JsonObject object = gcmdoc.as<JsonObject>();
    JsonArray layer_objects = object[F("l")];
    if (!layer_objects.isNull() && layer_objects.size() > 0) {
      for (uint8_t i = 0; i < layer_objects.size(); i++) {
        if(layer_objects[i].is<JsonVariant>()) {
          retval = load_layer(i, layer_objects[i]);
        }
      }
    }
  }

  return retval;
}

bool load_file(String type, String id) {
  bool retval = false;
  if (type == "cm") {
    retval = load_composite(id);
  }
  else if (type == "im") {
    retval = load_image_solo(id);
  }
  else if (type == "pl") {
    retval = load_from_playlist(id);
  }
  return retval;
}


bool load_from_playlist(String id) {
  static String resume_id;
  static uint32_t pm = 0;
  static uint32_t item_duration = 0;
  static uint8_t i = 0;
  static JsonArray playlist;
  static bool playlist_loaded = false;
  bool refresh_needed = false;

  if (id != "") {
    // should not do if (id != "" && id != resume_id)
    // because a playlist with an id matching resume_id may have been edited,
    // and therefore we would like to reload it to see the changes.
    // calling with an id means we want to load a new playlist so reinitialize everything
    gplaylist_enabled = false;
    resume_id = id;
    pm = 0; // using zero pm and item_duration ensures first item will be loaded immediately next time load_from_playlist() is called
    item_duration = 0;
    i = 0;
    playlist_loaded = false;

    String fs_path = form_path(F("pl"), id);
    File file = LittleFS.open(fs_path, "r");
    if (file && file.available()) {
      // 29 ms slower on average
      //String json = file.readString();
      //file.close();
      //DeserializationError error = deserializeJson(gpldoc, json);

      // 19 ms slower on average
      //String json = file.readString();
      //file.close();
      //DeserializationError error = deserializeJson(gpldoc, file);

      // fastest
      // is this still the fastest if the playlist is long?
      ReadBufferingStream bufferedFile(file, 64);
      DeserializationError error = deserializeJson(gpldoc, bufferedFile);
      file.close();
      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        gplaylist_enabled = false;
        return refresh_needed;
      }
      JsonObject object = gpldoc.as<JsonObject>();
      if (!object[F("pl")].isNull() && object[F("pl")].size() > 0) {
        playlist = object[F("pl")];
        gplaylist_enabled = true;
        playlist_loaded  = true;
        // instead of loading the playlist and then loading the first item
        // just load the playlist on this call, then the next call can load the first item
        // returning now means less time is spent in this function when a new playlist is loaded
        return refresh_needed;
      }
    }
  }

  if (playlist_loaded && (millis()-pm) > item_duration) {
    pm = millis();
    item_duration = 1000; // set to a safe value which will be replaced below

    if(playlist[i].is<JsonVariant>()) {
      JsonVariant item = playlist[i];
      if(load_file(item[F("t")], item[F("id")])) {
        if(item[F("d")].is<JsonInteger>()) {
          item_duration = item[F("d")];
        }
        refresh_needed = true;
      }
      else {
        item_duration = 0;
      }
      if (playlist.size() > 0) {
        i = (i+1) % playlist.size();
      }
    }
    else {
      gplaylist_enabled = false;
    }
  }
  return refresh_needed;
}

//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    //request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    String url = "http://";
    url += get_ip();
    url += "/config.htm";
    request->redirect(url);
  }
};

void espDelay(uint32_t ms) {
  esp_sleep_enable_timer_wakeup(ms * 1000);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
  esp_light_sleep_start();
}

bool attempt_connect(void) {
  bool attempt;
  preferences.begin("config", false);
  attempt = !preferences.getBool("create_ap", true);
  preferences.end();
  return attempt;
}

String get_ip(void) {
  return IP.toString();
}

String get_mdns_addr(void) {
  String mdns_addr = mdns_host;
  mdns_addr += ".local";
  return mdns_addr;
}

String processor(const String& var) {
  preferences.begin("config", false);
  if (var == "NUM_LAYERS")
    return String(NUM_LAYERS);

  if (var == "SSID")
    return preferences.getString("ssid", "");
  if (var == "MDNS_HOST")
    return preferences.getString("mdns_host", "");
  if (var == "NUM_ROWS")
    return String(NUM_ROWS);
  if (var == "NUM_COLS")
    return String(NUM_COLS);
  preferences.end();    
  return String();
}

void wifi_AP(void) {
  DEBUG_PRINTLN(F("Entering AP Mode."));
  //WiFi.softAP(SOFT_AP_SSID, "123456789");
  WiFi.softAP(SOFT_AP_SSID, "");
  
  IP = WiFi.softAPIP();

  DEBUG_PRINT(F("AP IP address: "));
  DEBUG_PRINTLN(IP);
}

bool wifi_connect(void) {
  bool success = false;

  //if (!WiFi.config(LOCAL_IP, GATEWAY, SUBNET_MASK, DNS1, DNS2)) {
  //  DEBUG_PRINTLN(F("WiFi config failed."));
  //}

  preferences.begin("config", false);

  String ssid;
  String password;

  ssid = preferences.getString("ssid", ""); 
  password = preferences.getString("password", "");

  DEBUG_PRINTLN(F("Entering Station Mode."));
  if (WiFi.SSID() != ssid.c_str()) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    WiFi.persistent(true);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
  }

  if (WiFi.waitForConnectResult(WIFI_CONNECT_TIMEOUT) == WL_CONNECTED) {
    DEBUG_PRINTLN(F(""));
    DEBUG_PRINT(F("Connected: "));
    IP = WiFi.localIP();
    DEBUG_PRINTLN(IP);
    success = true;
  }
  else {
    DEBUG_PRINT(F("Failed to connect to WiFi."));
    preferences.putBool("create_ap", true);
    success = false;
  }
  preferences.end();
  return success;
}

void mdns_setup(void) {
  preferences.begin("config", false);
  mdns_host = preferences.getString("mdns_host", "");

  if (mdns_host == "") {
    mdns_host = MDNS_HOSTNAME;
  }

  if(!MDNS.begin(mdns_host.c_str())) {
    DEBUG_PRINTLN(F("Error starting mDNS"));
  }
  preferences.end();
}

bool filterOnNotLocal(AsyncWebServerRequest *request) {
  // have to refer to service when requesting hostname from MDNS
  // but this code is not working for me.
  //DEBUG_PRINTLN(MDNS.hostname(1));
  //DEBUG_PRINTLN(MDNS.hostname(MDNS.queryService("http", "tcp")));
  //return request->host() != get_ip() && request->host() != MDNS.hostname(1); 

  return request->host() != get_ip() && request->host() != mdns_host;
}

void web_server_station_setup(void) {
  web_server.on("/save", HTTP_POST, [](AsyncWebServerRequest *request) {
    int rc = 400;
    String message;

    String type = request->getParam("t", true)->value();
    String id = request->getParam("id", true)->value();
    String json = request->getParam("json", true)->value();

    if (id != "") {
      String fs_path = form_path(type, id);
      if (save_data(fs_path, json, &message)) {
        gnextup.type = type;
        gnextup.id = id;
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
      gnextup.id = id;
      message = gnextup.id + " queued.";
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
    request->redirect("/remove.htm");
  });

  // files/ and www/ are both direct children of the littlefs root directory: /littlefs/files/ and /littlefs/www/
  // if the URL starts with /files/ then first look in /littlefs/files/ for the requested file
  web_server.serveStatic("/files/", LittleFS, "/files/");
  // since htm files are gzipped they cannot be run through the template processor. so extract variables that
  // we wish to set through template processing to non-gzipped js files. this has the added advantage of the
  // file being read through the template processor being much smaller and therefore quicker to process.
  web_server.serveStatic("/js", LittleFS, "/www/js").setTemplateProcessor(processor);
  // if the URL starts with / then first look in /littlefs/www/ for the requested page
  web_server.serveStatic("/", LittleFS, "/www/");

  web_server.onNotFound([](AsyncWebServerRequest *request) {
    if (request->method() == HTTP_OPTIONS) {
      request->send(200);
    } else {
      //request->send(404, "text/plain", "404"); // for testing
      request->redirect("/"); // will cause redirect loop if request handlers are not set up properly
    }
  });
}

void web_server_ap_setup(void) {
  // create a captive portal that catches every attempt to access data besides what the ESP serves to config.htm
  // requests to the ESP are handled normally
  // a captive portal makes it easier for a user to save their WiFi credentials to the ESP because they do not
  // need to know the ESP's IP address.
  gdns_up = dnsServer.start(53, "*", IP);
  web_server.addHandler(new CaptiveRequestHandler()).setFilter(filterOnNotLocal);

  // want limited access when in AP mode. AP mode is just for WiFi setup.
  web_server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/www/config.htm");
  });
}

void web_server_initiate(void) {

  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT");
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");

  // it is possible for more than one handler to serve a file
  // the first handler that matches a request will server the file
  // so need to put this before serveStatic(), otherwise serveStatic() will serve restart.htm, but not set grestart_needed to true;
  web_server.on("/restart.htm", HTTP_GET, [](AsyncWebServerRequest *request) {
    grestart_needed = true;
    request->send(LittleFS, "/www/restart.htm");
  });

  web_server.on("/saveconfig", HTTP_POST, [](AsyncWebServerRequest *request) {
    preferences.begin("config", false);

    if (request->hasParam("ssid", true)) {
      AsyncWebParameter* p = request->getParam("ssid", true);
      if (!p->value().isEmpty()) {
        preferences.putString("ssid", p->value().c_str());
      }
    }

    if (request->hasParam("password", true)) {
      AsyncWebParameter* p = request->getParam("password", true);
      if (!p->value().isEmpty()) {
        preferences.putString("password", p->value().c_str());
      }
    }

    if (request->hasParam("mdns_host", true)) {
      AsyncWebParameter* p = request->getParam("mdns_host", true);
      String mdns = p->value();
      mdns.replace(" ", ""); // autocomplete will add space to end of a word if phone is used to enter mdns hostname. remove it.
      mdns.toLowerCase();
      if (!mdns.isEmpty()) {
        preferences.putString("mdns_host", mdns.c_str());
      }
    }

    if (request->hasParam("rows", true)) {
      AsyncWebParameter* p = request->getParam("rows", true);
      uint8_t num_rows = p->value().toInt();
      if (0 < num_rows && num_rows <= 32) {
        preferences.putUChar("rows", num_rows);
      }
    }

    if (request->hasParam("columns", true)) {
      AsyncWebParameter* p = request->getParam("columns", true);
      uint8_t num_cols = p->value().toInt();
      if (0 < num_cols && num_cols <= 32) {
        preferences.putUChar("columns", num_cols);
      }
    }

    preferences.putBool("create_ap", false);

    preferences.end();

    request->redirect("/restart.htm");
  });


  if (ON_STA_FILTER) {
    web_server_station_setup();
  }
  else if (ON_AP_FILTER) {
    web_server_ap_setup();
  }

  web_server.begin();
}

void show(void) {
  static uint32_t pm = 0;
  static bool refresh_now = true;
  static bool image_has_transparency = true;

  // since web_server interrupts we have to queue changes instead of running them directly from web_server's on functions
  // otherwise changes we make could be undo once the interrupt hands back control which could be in the middle of code setting up a different animation
  if (gnextup.id) {
    if (gnextup.type == "pl") {
      // initialize playlist
      load_from_playlist(gnextup.id);
    }
    else if (gnextup.type == "cm") {
      gplaylist_enabled = false;
      load_composite(gnextup.id);
    }
    else if (gnextup.type == "im") {
      gplaylist_enabled = false;
      load_image_solo(gnextup.id);
    }
    gnextup.type = "";
    gnextup.id = "";
  }

  // draw layers. changes in layers are not displayed until they are copied to leds[] in the blend block
  for (uint8_t i = 0; i < NUM_LAYERS; i++) {
    if (layers[i] != nullptr) {
      layers[i]->reanimate();
    }
  }

  // blend block
  uint32_t dt = millis()-pm;
  if (dt > 100 || refresh_now) {
    pm = millis();
    //if (dt > 100) {
    //  DEBUG_PRINTLN(dt);
    //}
    refresh_now = false;
    gdynamic_hue+=3;
    gdynamic_rgb = CHSV(gdynamic_hue, 255, 255);
    gdynamic_comp_rgb = CRGB::White - gdynamic_rgb;
    grandom_hue = random8();

    FastLED.clear(); // use clear instead of tracking background layer.
    CRGBA pixel;
    for (uint8_t i = 0; i < NUM_LAYERS; i++) {
      if (layers[i] != nullptr) {
        //Serial.print("show: ");
        //Serial.println(i);
        for (uint16_t j = 0; j < NUM_LEDS; j++) {
          // transparency is used in two different ways here.
          // 1) if the pixels of the image have an alpha less than 100% (255) then they will be replaced with the background layer.
          // 2) !!NOT IMPLEMENTED!! Layer composer has an Alpha slider which is separate from the image's alpha channel. This slider controls how much of the
          //    non-transparent parts of the original image are blended with the layers below it.
          // in short 1) local/pixel level effect and 2) global effect
          //
          pixel = layers[i]->get_pixel(j);
          uint8_t alpha = pixel.a;
          CRGB bgpixel = leds[j];
          // before implementing pixel level transparency previously used a global alpha that is not currently implemented
          //alpha = scale8(alpha, gimage_layer_alpha);
          //if (alpha != 255) {
          //  bgpixel.fadeLightBy(64); // fading the background areas make the pixel art stand out
          //}
          leds[j] = nblend(bgpixel, (CRGB)pixel, alpha);
        }
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

  // load_from_playlist both initializes a playlist and shows a playlist item
  // initializing and showing the first playlist item takes some time, so it
  // is better to initialize, refresh the leds, then show the first playlist item
  // that is why load_from_playlist(gnextup.id) comes before the leds[] refresh code
  // and load_from_playlist() comes after it.
  if (gplaylist_enabled) {
    // show playlist item
    refresh_now = load_from_playlist();
  }

  FastLED.show();
}


void setup() {
  Serial.begin(115200);

  preferences.begin("config", false);
  NUM_ROWS = preferences.getUChar("rows", DEFAULT_NUM_ROWS);
  NUM_COLS = preferences.getUChar("columns", DEFAULT_NUM_COLS);
  //NUM_ROWS = 12; //testing
  //NUM_COLS = 12;
  preferences.end();
  NUM_LEDS = NUM_ROWS*NUM_COLS;
  leds = (CRGB*)malloc(NUM_ROWS*NUM_COLS*sizeof(CRGB));

  for (uint8_t i = 0; i < NUM_LAYERS; i++) {
    layers[i] = nullptr;
  }

  // setMaxPowerInVoltsAndMilliamps() should not be used if homogenize_brightness_custom() is used
  // since setMaxPowerInVoltsAndMilliamps() uses the builtin LED power usage constants 
  // homogenize_brightness_custom() was created to avoid.
  FastLED.setMaxPowerInVoltsAndMilliamps(LED_STRIP_VOLTAGE, LED_STRIP_MILLIAMPS);
  FastLED.setCorrection(TypicalSMD5050);
  FastLED.addLeds<WS2812B, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);

  FastLED.clear();
  FastLED.show(); // clear the matrix on startup

  homogenize_brightness();
  FastLED.setBrightness(homogenized_brightness);

  //random16_set_seed(analogRead(A0)); // use randomness ??? need to look up which pin for ESP32 ???

  if (!LittleFS.begin()) {
    DEBUG_PRINTLN("LittleFS initialisation failed!");
    while (1) yield(); // cannot proceed without filesystem
  }

  if (attempt_connect()) {
  	if (!wifi_connect()) {
	  	// failure to connect will result in creating AP
      espDelay(2000);
  		wifi_AP();
	  }
  }
  else {
    wifi_AP();
  }

  mdns_setup();
  web_server_initiate();

  handle_file_list(); // refresh file list before starting loop() because refreshing is slow

  while(!create_patterns_list());
  while(!create_accents_list());

  // initialzie dynamic colors because otherwise they won't be set until after layer.refresh() has been called which can lead to partially black text
  gdynamic_rgb = CHSV(gdynamic_hue, 255, 255);
  gdynamic_comp_rgb = CRGB::White - gdynamic_rgb;

  TaskHandle_t Task1;

  // use core 0 to load images to prevent lag
  xTaskCreatePinnedToCore(ReAnimator::load_image_from_queue, "Task1", 10000, NULL, 1, &Task1, 0);

  //load_file(F("pl"), "startup");
  load_file(F("im"), "bottle_magic");
}


void loop() {
  //static uint32_t pm = 0;
  //if ((millis()-pm) > 2000) {
  //  pm = millis();
  //  Serial.print("heap free: ");
  //  Serial.println(esp_get_free_heap_size());
  //}

  if (gdns_up) {
    dnsServer.processNextRequest();
  }

  if (grestart_needed || (WiFi.getMode() == WIFI_STA && WiFi.status() != WL_CONNECTED)) {
    delay(2000);
    ESP.restart();
  }

  show();

  handle_file_list();
  handle_delete_list();
}