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
#include <set>

#include "project.h"
#include "ReAnimator.h"

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

struct Timezone {
  // TZ is only set at boot, so it is possible for iana_tz and posix_tz to have been updated from default values, but not put into effect yet.
  // therefore we use a separate variable to track if the default timezone is in use instead of trying to do something like compare iana_tz or posix_tz to "" (empty string)
  bool is_default_tz;
  String iana_tz;
  // input from frontend, passed to timezoned.rop.nl for verification and fetching corresponding posix_tz
  // a separate varible is used instead of iana_tz to prevent losing previous data if verify_timezone() fails for some reason (e.g. unverified_iana_tz is an invalid timezone).
  String unverified_iana_tz; 
  String posix_tz;
} tz;

Preferences preferences;

bool restart_needed = false;
bool dns_up = false;

IPAddress IP;
String mdns_host;

// any changes to these values here will be overwritten
// these values are set in the frontend
// and their defaults are set in platformio.ini
uint8_t NUM_ROWS = 0;
uint8_t NUM_COLS = 0;
uint16_t NUM_LEDS = 0;
uint8_t ORIENTATION = 0;

CRGB* leds; // output

ReAnimator* layers[NUM_LAYERS];
uint8_t ghost_layers[NUM_LAYERS] = {0};

uint8_t gdynamic_hue = 0;
uint8_t grandom_hue = 0;
CRGB gdynamic_rgb = 0x000000;
CRGB gdynamic_comp_rgb = 0x000000; // complementary color to the dynamic color

uint8_t homogenized_brightness = 255;

bool playlist_enabled = false;

String art_type = "";

struct {
  String type;
  String id;
} ui_request;

const char* stored_file_list = FILE_ROOT "/file_list.txt";

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
void write_file_list_to_disk(void);
void update_file_list(bool action, String type, String id);
void create_file_list(void);
void delete_files(String type, String id);
void handle_delete_list(void);
bool create_patterns_list(void);
bool create_accents_list(void);
bool save_data(String type, String id, String json, String* message = nullptr);
void puck_man_cb(uint8_t event);
bool image_exists(String id);
bool is_valid_layer_json(JsonVariant layer_json);
bool load_layer(uint8_t lnum, JsonVariant layer_json);
bool load_image_to_layer(uint8_t lnum, String id, uint32_t image_duration = REFRESH_INTERVAL);
bool load_image_solo(String id);
bool load_collection(String type, String id);
bool load_from_playlist(String id = "");
bool load_file(String type, String id);
void handle_ui_request(void);
void write_log(String log_msg);

bool verify_timezone(const String iana_tz);
void esp_delay(uint32_t ms);
bool attempt_connect(void);
String get_ip(void);
String get_mdns_addr(void);
void wifi_AP(void);
bool wifi_connect(void);
void mdns_setup(void);
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


// NOTE: An empty folder will not be added when building a littlefs image.
// Empty folders will not be created when uploaded either.
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
  //update_file_list(1, path);
}


// set has slower access time but uses less memory
// access time should not be a problem because the number of entries will probably be at most in the low hundreds
std::set<std::string> gfile_list_set;
void write_file_list_to_disk(void) {
  File file = LittleFS.open(stored_file_list, "w");
  if (!file) {
    DEBUG_PRINTLN("Failed to open stored file list for writing.");
    return;
  }

  for (const auto& item : gfile_list_set) {
      // println() adds \r\n, so use print() and manually add newline to save disk space.
      file.print(item.c_str());
      file.print("\n");
  }

  file.close();
}


void update_file_list(bool action, String type, String id) {
  String fs_path = form_path(type, id, false);
  bool found = false;
  if (action) {
    gfile_list_set.insert(fs_path.c_str());
    found = true;
  }
  else {
    auto it = gfile_list_set.find(fs_path.c_str());
    if (it != gfile_list_set.end()) {
      found = true;
      gfile_list_set.erase(it);
    }
  }

  if (found) {
    write_file_list_to_disk();
  }
}


// this takes multiple seconds and halts the display so only call it when necessary
bool grebuild_file_list = false;
void create_file_list(void) {
  gfile_list_set.clear();
  gfile_list_set.insert("ROOT:" FILE_ROOT);

  File start_dir = LittleFS.open(FILE_ROOT);
  if (start_dir) {
    while (File parent = start_dir.openNextFile()) {
      String list_entry = "";
      if (parent.isDirectory()) {
        list_entry += parent.name();
        list_entry += "/";

        gfile_list_set.insert(list_entry.c_str());

        while (File child = parent.openNextFile()) {
          String id = child.name();
          id.remove(id.length()-5); // remove .json extension

          list_entry = parent.name();
          list_entry += "/";
          list_entry += id;

          gfile_list_set.insert(list_entry.c_str());

          child.close();
        }
        parent.close();
      }
    }
  }
  start_dir.close();

  write_file_list_to_disk();
}


void delete_files(String type, String id) {
  String fs_path = form_path(type, id, true);
  File entry1 = LittleFS.open(fs_path);
  if (entry1) {
    if (entry1.isDirectory()) {
      while (File entry2 = entry1.openNextFile()) {
        String filename = entry2.name();
        entry2.close();
        LittleFS.remove(fs_path+filename);
        filename.remove(filename.length()-5); // remove .json extension
        update_file_list(0, type, filename);
      }
      entry1.close();
      //LittleFS.rmdir(path);  // directories are used to indicate type and therefore should not be deleted
    }
    else {
      entry1.close();
      LittleFS.remove(fs_path);
      update_file_list(0, type, id);
    }
  }
  else {
    // file does not actually exist but is still on file list, so remove it
    update_file_list(0, type, id);
  }
}


String gdelete_list;
void handle_delete_list(void) {
  int f = gdelete_list.indexOf('\n');
  while (gdelete_list != "") {
    String data = gdelete_list.substring(0, f);
    int di = data.indexOf('/');  // file manager checkbox name uses slash to separate type and id
    if (di != -1) {
      String type = data.substring(0, di);
      String id = data.substring(di+1);
      delete_files(type, id);
    }
    else {
      delete_files(data, ""); // directory
    }
    if (f+1 < gdelete_list.length()) {
      gdelete_list = gdelete_list.substring(f+1);
      f = gdelete_list.indexOf('\n');
    }
    else {
      gdelete_list = "";
    }
  }
}


bool save_data(String type, String id, String json, String* message) {
  String fs_path = form_path(type, id, true);
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
    f.close();
    update_file_list(1, type, id);
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


std::set<std::string> load_file_list_from_disk(const char* filename) {
    std::set<std::string> result;
    File file = LittleFS.open(filename, "r");
    if (!file) {
        DEBUG_PRINTLN("Failed to open stored file list for reading.");
        return result;
    }

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        result.insert(line.c_str());
    }

    file.close();
    return result;
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
    case RAIN:
        pattern_name = "Rain";
        match = true;
        break;
    case WATERFALL:
        pattern_name = "Waterfall";
        match = true;
        break;
    case XRAY_SPARKLE:
        pattern_name = "X-ray Sparkle";
        match = true;
        break;
    case XRAY_ORBIT:
        pattern_name = "X-ray Orbit";
        match = true;
        break;
    case XRAY_SCAN:
        pattern_name = "X-ray Scan";
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
  static Accent accent_id = static_cast<Accent>(0);
  String accent_name;
  bool match = false;
  static bool first = true;
  bool finished = false;
  switch(accent_id) {
    default:
        if (accent_id > 49) {
          accent_id = static_cast<Accent>(0);
          finished = true;
        }
        break;
    // Since accents are an optional, secondary effect it makes sense to present an option to have no accent in the frontend
    case NO_ACCENT:
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
    accent_id = static_cast<Accent>(accent_id+1);
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
          load_image_to_layer(i, "ghost_blinky");
        }
        if (ghost_layers[i] == 2) {
          load_image_to_layer(i, "ghost_pinky");
        }
        if (ghost_layers[i] == 3) {
          load_image_to_layer(i, "ghost_inky");
        }
        if (ghost_layers[i] == 4) {
          load_image_to_layer(i, "ghost_clyde");
        }
      }
      break;
    case 1:
      if (!one_shot) {
        one_shot = true;
        for (uint8_t i = 0; i < NUM_LAYERS; i++) {
          if (ghost_layers[i]) {
            load_image_to_layer(i, "ghost_blue");
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


bool image_exists(String id) {
  // the file_list is used instead of LittleFS.exists() because exists() is a thousand or more times slower.
  String entry = "im";
  entry += "/";
  entry += id;
  return gfile_list_set.find(entry.c_str()) != gfile_list_set.end();
}


bool is_valid_layer_json(JsonVariant layer_json) {
  if (layer_json[F("t")] == "e") {
    return false;
  }
  if (layer_json[F("t")].isNull()) {
    return false;
  }
  if (layer_json[F("id")].isNull()) {
    return false;
  }
  if (layer_json[F("t")] == "w" && layer_json[F("w")].isNull()) {
    return false;
  }
  if (layer_json[F("t")] == "im" && !image_exists(layer_json[F("id")])) {
    return false;
  }
  return true;
}


bool load_layer(uint8_t lnum, JsonVariant layer_json) {
  if (!is_valid_layer_json(layer_json)) {
    if (layers[lnum] != nullptr) {
      delete layers[lnum];
      layers[lnum] = nullptr;
    }
    return false;
  }

  if (layers[lnum] == nullptr) {
    layers[lnum] = new ReAnimator(NUM_ROWS, NUM_COLS, ORIENTATION);
  }

  // sane defaults in case data is missing.
  // if this data is missing from layer_json perhaps it is better to not show the layer at all.
  uint8_t accent_id = 0;
  uint8_t color_type = 1; // dynamic color
  uint32_t color = 0x000000;
  uint8_t movement = 0; // no movement
  uint32_t image_duration = REFRESH_INTERVAL;

  if (!layer_json[F("a")].isNull()) {
    accent_id = layer_json[F("a")];
  }

  if (!layer_json[F("c")].isNull()) {
    if (layer_json[F("c")].is<const char*>()) {
      // fixed colors are stored as RGB hex in a string
      const char* cs = layer_json[F("c")].as<const char*>();
      if (strlen(cs) == 8 && cs[1] == 'x') {
        color_type = 0;
        color = strtoul(cs, NULL, 16);
      }
    }
    else if (layer_json[F("c")].is<uint8_t>()) {
      // dynamic colors are indicated by a number.
      // might be able to use other number to indicate a particular color palette should be used.
      color_type = layer_json[F("c")].as<uint8_t>();
    }
  }
  
  // the color setting of a layer is used for setting the color of a pattern and text
  // but it is also used to replace the proxy color of an image

  if (color_type == 0) {          
    layers[lnum]->set_color(color);
  }
  else if (color_type == 2) {
    layers[lnum]->set_color(&gdynamic_comp_rgb);
  }
  else {
    layers[lnum]->set_color(&gdynamic_rgb);
  }

  if (!layer_json[F("m")].isNull()) {
    movement = layer_json[F("m")];
  }

  if (!layer_json[F("d")].isNull()) {
    image_duration = layer_json[F("d")];
  }

  ghost_layers[lnum] = 0;
  if (layer_json[F("t")] == "im") {
    String id = layer_json[F("id")];
    if (id == "ghost_blinky") {
      ghost_layers[lnum] = 1;
    }
    if (id == "ghost_pinky") {
      ghost_layers[lnum] = 2;
    }
    if (id == "ghost_inky") {
      ghost_layers[lnum] = 3;
    }
    if (id == "ghost_clyde") {
      ghost_layers[lnum] = 4;
    }
    layers[lnum]->setup(Image_t, -2);
    if (!load_image_to_layer(lnum, id, image_duration)) {
      return false;
    }
    layers[lnum]->set_accent(static_cast<Accent>(accent_id), true);
    layers[lnum]->set_heading(movement);
  }
  else if (layer_json[F("t")] == "p") {
    uint8_t id = layer_json[F("id")];
    layers[lnum]->setup(Pattern_t, id);
    layers[lnum]->set_cb(&puck_man_cb);
    layers[lnum]->set_pattern(static_cast<Pattern>(id));
    layers[lnum]->set_accent(static_cast<Accent>(accent_id), true);
    layers[lnum]->set_heading(movement);
  }
  else if (layer_json[F("t")] == "w") {
    layers[lnum]->setup(Text_t, -1);
    layers[lnum]->set_text(layer_json[F("w")]);
    layers[lnum]->set_accent(static_cast<Accent>(accent_id), true);
    // direction is disabled for text in the frontend. setting to default of 0.
    // if it is not set back to 0 on a layer that previously had movement the text
    // will move.
    layers[lnum]->set_heading(movement);
  }
  else if (layer_json[F("t")] == "n") {
    uint8_t id = layer_json[F("id")];
    layers[lnum]->setup(Info_t, id);
    layers[lnum]->set_info(static_cast<Info>(id));
    layers[lnum]->set_accent(static_cast<Accent>(accent_id), true);
    layers[lnum]->set_heading(movement);
  }
  return true;
}

// this checks the layer and image exists before loading an image to it.
// the idea is that the layer exists and we want to preserve all
// its other attributes but replace its image.
// this helps streamline code from having the same repeative layer and image
// existence checks.
bool load_image_to_layer(uint8_t lnum, String id, uint32_t image_duration) {
  if (layers[lnum] != nullptr) {
    if (image_exists(id)) {
      layers[lnum]->set_image(id, image_duration);
      // since the image is loaded asynchronously using another core to prevent lag
      // waiting to determine if the image was loaded successfully would defeat the purpose.
      // so the best we can do is check if the image exists for indicating success.
      // this file existence check helps playlist handling perform better because it means
      // non-existent images can be skipped.
      return true;
    }
    delete layers[lnum];
    layers[lnum] = nullptr;
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
    layers[0] = new ReAnimator(NUM_ROWS, NUM_COLS, ORIENTATION);
    layers[0]->setup(Image_t, -2);
    retval = load_image_to_layer(0, id);
    if (retval) {
      layers[0]->set_color(&gdynamic_rgb);
      layers[0]->set_heading(0);
    }
  }

  return retval;
}


bool load_collection(String type, String id) {
  bool retval = false;
  String fs_path = form_path(type, id, true);
  File file = LittleFS.open(fs_path, "r");
  
  if (!file){
    return false;
  }

  if (file.available()) {
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
          retval = load_layer(i, layer_objects[i]) || retval;
        }
      }
    }
  }

  return retval;
}


bool load_from_playlist(String id) {
  const uint32_t min_duration = 200;
  bool refresh_needed = false;
  if (playlist_enabled) {
    static String resume_id;
    static uint32_t pm = 0;
    static uint32_t item_duration = 0;
    static uint8_t i = 0;
    static JsonArray playlist;
    static bool playlist_loaded = false;

    if (id != "") {
      // should not do if (id != "" && id != resume_id)
      // because a playlist with an id matching resume_id may have been edited,
      // and therefore we would like to reload it to see the changes.
      // calling with an id that is not blank means we want to load a new playlist so reinitialize everything
      playlist_enabled = false;
      resume_id = id;
      pm = 0; // using zero pm and item_duration ensures first item will be loaded immediately next time load_from_playlist() is called
      item_duration = 0;
      i = 0;
      playlist_loaded = false;

      String fs_path = form_path(F("pl"), id, true);
      File file = LittleFS.open(fs_path, "r");
      if (file && file.available()) {
        ReadBufferingStream bufferedFile(file, 64);
        DeserializationError error = deserializeJson(gpldoc, bufferedFile);
        file.close();
        if (error) {
          DEBUG_PRINT("deserializeJson() failed: ");
          DEBUG_PRINTLN(error.c_str());
          playlist_enabled = false;
          return refresh_needed;
        }
        JsonObject object = gpldoc.as<JsonObject>();
        if (!object[F("pl")].isNull() && object[F("pl")].size() > 0) {
          playlist = object[F("pl")];
          playlist_enabled = true;
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
            // it takes a bit under 100 ms to load an image
            // item_durations of around 100 ms and less can cause the display to appear stalled or act erratic and causes a crash
            // therefore the minimum item_duration is limited to 200 ms
            item_duration = max(item_duration, min_duration);
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
        playlist_enabled = false;
      }
    }
  }
  return refresh_needed;
}


bool load_file(String type, String id) {
  bool retval = false;
  art_type = type;
  if (type == "im") {
    retval = load_image_solo(id);
  }
  else if (type == "cm") {
    retval = load_collection(type, id);
  }
  else if (type == "an") {
    retval = load_collection(type, id);
  }
  else if (type == "pl") {
    art_type = "";
    playlist_enabled = true;
    // initialize playlist
    retval = load_from_playlist(id);
  }
  return retval;
}


void handle_ui_request(void) {
  // since web_server interrupts we have to queue changes instead of running them directly from web_server's on functions
  // otherwise changes we make could be undo once the interrupt hands back control which could be in the middle of code setting up a different animation
  if (ui_request.id.length() > 0) {
    playlist_enabled = false;
    if (ui_request.type == "pl") {
      playlist_enabled = true;
    }

    load_file(ui_request.type, ui_request.id);
    ui_request.type = "";
    ui_request.id = "";
  }
}


void write_log(String log_msg) {
  File f = LittleFS.open("/files/debug_logC.txt", "r");
  if (f) {
    size_t fsize = f.size();
    f.close();
    // if full, rotate log files
    if (fsize > 1750) {
      LittleFS.remove("/files/debug_logP.txt");
      LittleFS.rename("/files/debug_logC.txt", "/files/debug_logP.txt");
    }
  }

  f = LittleFS.open("/files/debug_logC.txt", "a");
  if (f) {
    struct tm local_now = {0};
    getLocalTime(&local_now, 0);
    char ts[23];
    snprintf(ts, sizeof ts, "%d/%02d/%02d %02d:%02d:%02d - ", local_now.tm_year+1900, local_now.tm_mon+1, local_now.tm_mday, local_now.tm_hour, local_now.tm_min, local_now.tm_sec);
    
    //noInterrupts(); // causes crash
    f.print(ts);
    f.print(log_msg);
    f.print("\n");
    f.close();
    //interrupts();
  }
}


//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////
//////////////////////////////////////////////

// the following code is taken from the ezTime library
// https://github.com/ropg/ezTime
/*
MIT License

Copyright (c) 2018 R. Gonggrijp

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
#define TIMEZONED_REMOTE_HOST  "timezoned.rop.nl"
#define TIMEZONED_REMOTE_PORT  2342
#define TIMEZONED_LOCAL_PORT   2342
#define TIMEZONED_TIMEOUT      2000 // milliseconds
String _server_error = "";

// instead of just the IANA timezone, technically a country code or empty string can also be input
// if an empty string is used a geolocate is done on the IP address
// however country codes and IP geolocates do not work for countries spanning multiple timezones
// so for simplicity we only input IANA timezones
bool verify_timezone(const String iana_tz) {
  tz.unverified_iana_tz = "";
  WiFiUDP udp;
  
  udp.flush();
  udp.begin(TIMEZONED_LOCAL_PORT);
  unsigned long started = millis();
  udp.beginPacket(TIMEZONED_REMOTE_HOST, TIMEZONED_REMOTE_PORT);
  udp.write((const uint8_t*)iana_tz.c_str(), iana_tz.length());
  udp.endPacket();
  
  // Wait for packet or return false with timed out
  while (!udp.parsePacket()) {
    delay (1);
    if (millis() - started > TIMEZONED_TIMEOUT) {
      udp.stop();  
      DEBUG_PRINTLN("verify_timezone(): fetch timezone timed out.");
      return false;
    }
  }

  // Stick result in String recv 
  String recv;
  recv.reserve(60);
  while (udp.available()) recv += (char)udp.read();
  udp.stop();
  DEBUG_PRINT(F("verify_timezone(): (round-trip "));
  DEBUG_PRINT(millis() - started);
  DEBUG_PRINTLN(F(" ms)  "));
  if (recv.substring(0,6) == "ERROR ") {
    _server_error = recv.substring(6);
    return false;
  }
  if (recv.substring(0,3) == "OK ") {
    //tz.is_default_tz = false; // TZ value is only set on boot, so default is still in effect until timezone is saved on config page and restart occurs
    tz.iana_tz = recv.substring(3, recv.indexOf(" ", 4));
    tz.posix_tz = recv.substring(recv.indexOf(" ", 4) + 1);
    return true;
  }
  DEBUG_PRINTLN("verify_timezone(): timezone not found.");
  return false;
}
// end ezTime MIT licensed code


class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    String url = "http://";
    url += get_ip();
    url += "/config.htm";
    request->redirect(url);
  }
};


void esp_delay(uint32_t ms) {
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
    String message = "Unknown error.";

    String type = request->getParam("t", true)->value();
    String id = request->getParam("id", true)->value();
    String json = request->getParam("json", true)->value();
    String load = "true";
    if (request->hasParam("load", true)) {
      // only the call to /save from the restore page sets the load parameter
      // restore sets it to false to prevent loading the file to display after saving
      // since the restore process saves many files at once
      load = request->getParam("load", true)->value();
    }

    if (id != "") {
      if (save_data(type, id, json, &message)) {
        if (load == "true") {
          ui_request.type = type;
          ui_request.id = id;
        }
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

    if (id != "" && (type == "im" || type == "cm" || type == "an" || type == "pl")) {
      ui_request.type = type;
      ui_request.id = id;
      message = ui_request.id + " queued.";
      rc = 200;
    }
    else {
      message = "Invalid type.";
    }

    request->send(rc, "application/json", "{\"message\": \""+message+"\"}");
  });

  web_server.on("/options.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    String options_json = "{\"patterns\":["+patterns_json + "],\"accents\":["+accents_json + "]}"; 
    request->send(200, "application/json", options_json);
  });

  web_server.on("/file_list", HTTP_GET, [](AsyncWebServerRequest *request) {
    File file = LittleFS.open(stored_file_list, "r");
    if (!file || file.isDirectory()) {
      request->send(404, "text/plain", "File not found");
      return;
    }

    request->send(file, "text/plain");
  });

  web_server.on("/rebuild_file_list", HTTP_GET, [](AsyncWebServerRequest *request) {
    grebuild_file_list = true;
    request->send(200, "application/json", "{\"message\": \"rebuild request received\"}");
  });

  web_server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request) {
    int params = request->params();
    for(int i=0; i < params; i++){
      AsyncWebParameter* p = request->getParam(i);
      if(p->isPost()){
        DEBUG_PRINTF("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());

        gdelete_list += p->name();
        gdelete_list += "\n";
      }
    }
    request->redirect("/file_manager.htm");
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
      request->send(404, "text/plain", "404 - NOT FOUND");
      //request->redirect("/"); // will cause redirect loop if request handlers are not set up properly
    }
  });
}


void web_server_ap_setup(void) {
  // create a captive portal that catches every attempt to access data besides what the ESP serves to config.htm
  // requests to the ESP are handled normally
  // a captive portal makes it easier for a user to save their WiFi credentials to the ESP because they do not
  // need to know the ESP's IP address.
  dns_up = dnsServer.start(53, "*", IP);
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
  // the first handler that matches a request will serve the file
  // so need to put this before serveStatic(), otherwise serveStatic() will serve restart.htm, but not set restart_needed to true;
  web_server.on("/restart.htm", HTTP_GET, [](AsyncWebServerRequest *request) {
    restart_needed = true;
    request->send(LittleFS, "/www/restart.htm");
  });

  web_server.on("/verify_timezone", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("iana_tz", true)) {
      AsyncWebParameter* p = request->getParam("iana_tz", true);
      if (!p->value().isEmpty()) {
        tz.unverified_iana_tz = p->value().c_str();
        request->send(200);
      }
      else {
        request->send(400);
      }
    }
    else {
      request->send(400);
    }
  });

  web_server.on("/get_ip", HTTP_GET, [](AsyncWebServerRequest *request) {
    String ip_json = "{\"IP\":\"";
    ip_json += get_ip();
    ip_json += "\"}";
    request->send(200, "application/json", ip_json);
  });

  web_server.on("/get_timezone", HTTP_GET, [](AsyncWebServerRequest *request) {
    String timezone = "{\"is_default_tz\":";
    String str_is_default_tz = (tz.is_default_tz) ? "true" : "false";
    timezone += str_is_default_tz;
    timezone += ",\"iana_tz\":\"";
    timezone += tz.iana_tz;
    timezone += "\",\"posix_tz\":\"";
    timezone += tz.posix_tz;
    timezone += "\"}";
    request->send(200, "application/json", timezone);
  });

  web_server.on("/get_config", HTTP_GET, [](AsyncWebServerRequest *request) {
    preferences.begin("config", true);
    String config = "{\"ssid\":\"";
    config += preferences.getString("ssid", "");
    config += "\",\"mdns_host\":\"";
    config += preferences.getString("mdns_host", "");
    config += "\",\"rows\":\"";
    config += preferences.getUChar("rows", DEFAULT_NUM_ROWS);
    config += "\",\"columns\":\"";
    config += preferences.getUChar("columns", DEFAULT_NUM_ROWS);
    config += "\",\"orientation\":\"";
    config += preferences.getUChar("orientation", DEFAULT_ORIENTATION);
    config += "\"}";
    preferences.end();
    DEBUG_PRINTLN(config);
    request->send(200, "application/json", config);
  });

  web_server.on("/save_config", HTTP_POST, [](AsyncWebServerRequest *request) {
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

    if (request->hasParam("orientation", true)) {
      AsyncWebParameter* p = request->getParam("orientation", true);
      uint8_t orientation = p->value().toInt();
      preferences.putUChar("orientation", orientation);
    }

    if (request->hasParam("iana_tz", true)) {
      AsyncWebParameter* p = request->getParam("iana_tz", true);
      if (!p->value().isEmpty()) {
        preferences.putString("iana_tz", p->value().c_str());
      }
    }

    if (request->hasParam("posix_tz", true)) {
      AsyncWebParameter* p = request->getParam("posix_tz", true);
      if (!p->value().isEmpty()) {
        preferences.putString("posix_tz", p->value().c_str());
      }
    }

    preferences.putBool("create_ap", false);

    preferences.end();

    request->redirect("/restart.htm");
  });

  // WiFi scanning code taken from ESPAsyncWebServer examples
  // https://github.com/me-no-dev/ESPAsyncWebServer?tab=readme-ov-file#scanning-for-available-wifi-networks
  // Copyright (c) 2016 Hristo Gochkov. All rights reserved.
  // This WiFi scanning code snippet is under the GNU Lesser General Public License.
  web_server.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
    int n = WiFi.scanComplete();
    if (n == -2) {
      WiFi.scanNetworks(true, true); // async scan, show hidden
    } else if (n) {
      for (int i = 0; i < n; ++i) {
        if (i) json += ",";
        json += "{";
        json += "\"rssi\":"+String(WiFi.RSSI(i));
        json += ",\"ssid\":\""+WiFi.SSID(i)+"\"";
        json += ",\"bssid\":\""+WiFi.BSSIDstr(i)+"\"";
        json += ",\"channel\":"+String(WiFi.channel(i));
        json += ",\"secure\":"+String(WiFi.encryptionType(i));
        //json += ",\"hidden\":"+String(WiFi.isHidden(i)?"true":"false"); // ESP32 does not support isHidden()
        json += "}";
      }
      WiFi.scanDelete();
      if(WiFi.scanComplete() == -2){
        WiFi.scanNetworks(true);
      }
    }
    json += "]";
    request->send(200, "application/json", json);
    json = String();
  });

  if (ON_STA_FILTER) {
    web_server_station_setup();
  }
  else if (ON_AP_FILTER) {
    web_server_ap_setup();
  }

  web_server.begin();
}
// end WiFi scanning code taken from ESPAsyncW3ebServer examples


void show(bool refresh_now) {
  static uint32_t pm = 0;

  bool images_waiting = false;
  for (uint8_t i = 0; i < NUM_LAYERS; i++) {
    if (layers[i] != nullptr) {
      // draw layer. changes in layers are not displayed until they are copied to leds[] in the blend block
      layers[i]->reanimate();

      if (layers[i]->get_type() == Image_t) {
        // to prevent flickering do not show layers until all images are loaded.
        int8_t image_status = layers[i]->get_image_status();
        if (image_status == 0) {
          images_waiting = true;
        }
      }
    }
  }

  // blend block
  uint32_t dt = millis()-pm;
  static uint32_t refresh_interval = REFRESH_INTERVAL;
  if ((dt > refresh_interval || refresh_now) && !images_waiting) {
    pm = millis();
    //if (dt > REFRESH_INTERVAL) {
    //  DEBUG_PRINTLN(dt);
    //}
    gdynamic_hue+=3;
    gdynamic_rgb = CHSV(gdynamic_hue, 255, 255);
    gdynamic_comp_rgb = CRGB::White - gdynamic_rgb;
    grandom_hue = random8();

    bool first_layer = true;
    CRGBA pixel;
    static uint8_t i = 0;
    while (true) {
      if (layers[i] != nullptr) {
        if (layers[i]->get_type() == Image_t) {
          // it is possible the image may never load, so after so many attempts the image was
          // marked as broken (-1) by get_image_status()
          int8_t image_status = layers[i]->get_image_status();
          if (image_status == -1) {
            continue; // skip showing the image, but show the rest of the layers.
          }
        }
        if (first_layer) {
          first_layer = false;
          refresh_now = false;
          // data in leds[] is written to a black background for first layer
          // tried white and black bitmasks but clear() is around 10-50 microseconds faster
          FastLED.clear();
        }
        for (uint16_t j = 0; j < NUM_LEDS; j++) {
          pixel = layers[i]->get_pixel(j);
          CRGB bgpixel = leds[j];
          if (layers[i]->is_xray_pattern()) {
            // most effects have active pixels that are colored and are opaque or semitransparent.
            // the active pixels are surrounded by negative space which is fully transparent black.
            // this allows layers to be drawn on top of each other to combine effects.
            // for xray patterns we want an opaque negative space which hides what is underneath and
            // active pixels that reveal what is underneath.
            // xray patterns are created the same as regular patterns (i.e. transparent negative space)
            // and then converted to have an opaque negative space and active pixels that are effectively
            // transparent in this block.

            uint8_t gray_value = (bgpixel.r + bgpixel.g + bgpixel.b) / 3;

            bool is_colored = ((CRGB)pixel != (CRGB)0);
            if (is_colored) {
              // use the gray scale value of the pixel below to adjust the pattern's color such that the effect
              // from the layer below is shown in a new color for the composite
              nscale8x3(pixel.r, pixel.g, pixel.b, gray_value);
              // data for bgpixel has been transferred to pixel, so set it to black so nblend() produces the correct output
              bgpixel = CRGB::Black;
            }
            else {
              // else pixel is fully transparent black or semitransparent black.
              // if it is fully transparent, then it is negative space, so hide everything beneath by setting alpha to 255 (fully opaque).
              // if it is opaque black or semi transparent black, then it is an active pixel, so invert its transparency so the pixels
              // underneath can be seen as is.

              // flipping the opaqueness has the effect of only showing the layer underneath
              // when the pixels of the layer above are not completely transparent.
              // that is completely transparent areas become completely opaque and hide what is underneath.
              pixel.a = 255 - pixel.a;
            }
          }
          // effectively merge pixel that is combination of previous layers with pixel from current layer. flatten.
          leds[j] = nblend(bgpixel, (CRGB)pixel, pixel.a);
        }
        refresh_interval = layers[i]->display_duration;
      }
      else if (art_type == "an") {
        // empty (nullptr) layers need to have a duration of 0 in order to not stall the gif-like animation
        refresh_interval = 0;
      }

      i = (i + 1) % NUM_LAYERS;
      if (i == 0 || art_type == "an") {
        break;
      }
    }

#if HOMOGENIZE_BRIGHTNESS
    homogenize_brightness();
#endif
    // safety measure while testing
    //if (homogenized_brightness > 128) {
    //  DEBUG_PRINT("homogenized_brightness > 128: ");
    //  DEBUG_PRINTLN(homogenized_brightness);
    //  homogenized_brightness = 128;
    //}

    FastLED.setBrightness(homogenized_brightness);
  }

  FastLED.show();
}


void setup() {
  DEBUG_BEGIN(115200);

  preferences.begin("config", false);
  NUM_ROWS = preferences.getUChar("rows", DEFAULT_NUM_ROWS);
  NUM_COLS = preferences.getUChar("columns", DEFAULT_NUM_COLS);
  ORIENTATION = preferences.getUChar("orientation", DEFAULT_ORIENTATION);
  NUM_LEDS = NUM_ROWS*NUM_COLS;
  leds = (CRGB*)malloc(NUM_ROWS*NUM_COLS*sizeof(CRGB));

  for (uint8_t i = 0; i < NUM_LAYERS; i++) {
    layers[i] = nullptr;
  }

  //The POSIX format is TZ = local_timezone,date/time,date/time.
  //Here, date is in the Mm.n.d format, where:
  //    Mm (1-12) for 12 months
  //    n (1-5) 1 for the first week and 5 for the last week in the month
  //    d (0-6) 0 for Sunday and 6 for Saturday

  //https://www.di-mgt.com.au/wclock/help/wclo_tzexplain.html
  //[America/New_York]
  //TZ=EST5EDT,M3.2.0/2,M11.1.0
  //
  //EST = designation for standard time when daylight saving is not in force
  //5 = offset in hours = 5 hours west of Greenwich meridian (i.e. behind UTC)
  //EDT = designation when daylight saving is in force (if omitted there is no daylight saving)
  //, = no offset number between code and comma, so default to one hour ahead for daylight saving
  //M3.2.0 = when daylight saving starts = the 0th day (Sunday) in the second week of month 3 (March)
  ///2, = the local time when the switch occurs = 2 a.m. in this case
  //M11.1.0 = when daylight saving ends = the 0th day (Sunday) in the first week of month 11 (November). No time is given here so the switch occurs at 02:00 local time.
  //
  //So daylight saving starts on the second Sunday in March and finishes on the first Sunday in November. The switch occurs at 02:00 local time in both cases. This is the default switch time, so the /2 isn't strictly needed. 
  //
  // ESP32 time.h library does not support setting TZ to IANA timezones. POSIX timezones (i.e. proleptic format) are required.
  tz.is_default_tz = false;
  tz.iana_tz = preferences.getString("iana_tz", "");
  tz.unverified_iana_tz = "";
  tz.posix_tz = preferences.getString("posix_tz", "");
  if (tz.posix_tz == "") {
    tz.is_default_tz = true;
    // US eastern timezone for TESTING
    //tz.iana_tz = "America/New_York";
    //tz.posix_tz = "EST5EDT,M3.2.0,M11.1.0";
    tz.iana_tz = "Etc/UTC";
    tz.posix_tz = "UTC0"; // "" has the same effect as UTC0
  }
  preferences.end();


  // setMaxPowerInVoltsAndMilliamps() should not be used if homogenize_brightness_custom() is used
  // since setMaxPowerInVoltsAndMilliamps() uses the builtin LED power usage constants 
  // homogenize_brightness_custom() was created to avoid.
  FastLED.setMaxPowerInVoltsAndMilliamps(LED_STRIP_VOLTAGE, LED_STRIP_MILLIAMPS);
  FastLED.setCorrection(TypicalSMD5050);
  FastLED.addLeds<WS2812B, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setDither(0); // disable temporal dithering. otherwise get flickering for dim pixels.

  FastLED.clear();
  FastLED.show(); // clear the matrix on startup

  homogenize_brightness();
  FastLED.setBrightness(homogenized_brightness);

  random16_set_seed(analogRead(A0));

  if (!LittleFS.begin()) {
    DEBUG_PRINTLN("LittleFS initialisation failed!");
    while (1) yield(); // cannot proceed without filesystem
  }

  // scanNetworks() only returns results the second time it is called, so call it here so when it is called again by the config page results will be returned
  WiFi.scanNetworks(false, true); // synchronous scan, show hidden

  if (attempt_connect()) {
    if (!wifi_connect()) {
      // failure to connect will result in creating AP
      esp_delay(2000);
      wifi_AP();
    }
  }
  else {
    wifi_AP();
  }

  mdns_setup();
  web_server_initiate();


  DEBUG_PRINT("Attempting to fetch time from ntp server.");
  configTzTime(tz.posix_tz.c_str(), "pool.ntp.org");
  struct tm local_now = {0};
  uint8_t attempt_cnt = 0;
  // we want the code to give up because having the time is not essential
  // the time is needed if a composite has a time effect but there is no guarantee a time effect is used
  // having the correct time is also useful for debug logging but not essential
  // if a time effect is used getLocalTime() will be called which might succeed in fetching the ntp time
  // even after this block has given up.
  uint8_t give_up_after = 2; // seconds (approximately)
  while (false) {  // temporarily turn off time lookup for quicker testing
    time_t now;
    time(&now);
    localtime_r(&now, &local_now);
    if(local_now.tm_year > (2016 - 1900)){
      break;
    }
    delay(10);
    attempt_cnt++;
    if (attempt_cnt == 100) {
      attempt_cnt = 0;
      DEBUG_PRINT(".");
      give_up_after--;
      if (!give_up_after) {
        break;
      }
    }
  }
  DEBUG_PRINTF("\n***** local time: %d/%02d/%02d %02d:%02d:%02d *****\n", local_now.tm_year+1900, local_now.tm_mon+1, local_now.tm_mday, local_now.tm_hour, local_now.tm_min, local_now.tm_sec);

  if (LittleFS.exists(stored_file_list)) {
    // about 50 milliseconds to load from disk
    gfile_list_set.clear();
    gfile_list_set = load_file_list_from_disk(stored_file_list);
    if (gfile_list_set.empty()) {
      // if only line is header (ROOT:...), then frontend javascript code works fine whether header is followed by newline or not
      gfile_list_set.insert("ROOT:" FILE_ROOT "\n");
    }
  }
  else {
    // about 3 to 5 seconds for about 100 files:
    create_file_list();
  }

  while(!create_patterns_list());
  while(!create_accents_list());

  // initialzie dynamic colors because otherwise they won't be set until after layer.refresh() has been called which can lead to partially black text
  gdynamic_rgb = CHSV(gdynamic_hue, 255, 255);
  gdynamic_comp_rgb = CRGB::White - gdynamic_rgb;

  TaskHandle_t Task1;

  // use core 0 to load images to prevent lag
  xTaskCreatePinnedToCore(ReAnimator::load_image_from_queue, "Task1", 10000, NULL, 1, &Task1, 0);
  
  load_file(F("pl"), "startup");

#if DEBUG_LOG == 1
  write_log("setup finished");
#endif
}


void loop() {

#if defined(DEBUG_CONSOLE) || DEBUG_LOG == 1
  char heap_free[18];
  static uint8_t hp_cnt = 0;
  static uint32_t pm = 0;
  if ((millis()-pm) > 2000) {
    pm = millis();
    snprintf(heap_free, sizeof(heap_free), "heap free: %lu", esp_get_free_heap_size());

#if defined(DEBUG_CONSOLE)
    DEBUG_PRINTLN(heap_free);
#endif

#if DEBUG_LOG == 1
    if (hp_cnt == 0) {
      write_log(heap_free);
    }
    hp_cnt++;
#endif
  }
#endif


  if (dns_up) {
    dnsServer.processNextRequest();
  }

  if (restart_needed || (WiFi.getMode() == WIFI_STA && WiFi.status() != WL_CONNECTED)) {
    delay(2000);
    ESP.restart();
  }

  bool refresh_now = load_from_playlist();
  show(refresh_now);

  if (grebuild_file_list) {
    grebuild_file_list = false;
    create_file_list();
  }

  handle_delete_list();
  handle_ui_request();

  if (tz.unverified_iana_tz != "") {
    verify_timezone(tz.unverified_iana_tz);
  }
}
