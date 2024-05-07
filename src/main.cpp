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

#include "ArduinoJson-v6.h"
#include "minjson.h"

#include "project.h"
//#include "ReAnimator.h"

#include "Layer.h"

//#include "credentials.h" // set const char *wifi_ssid and const char *wifi_password in include/credentials.h
#include "credentials_private.h"

#define DATA_PIN 16
#define COLOR_ORDER GRB

#define NUM_LAYERS 4

//#define TRANSPARENT (uint32_t)0x424242
#define COLORSUB (uint32_t)0x004200


AsyncWebServer web_server(80);

CRGB leds[NUM_LEDS] = {0}; // output

Layer* onions[NUM_LAYERS];


uint8_t gmax_brightness = 255;
//const uint8_t gmin_brightness = 2;
uint8_t gdynamic_hue = 0;
uint8_t gstatic_hue = HUE_ALIEN_GREEN;
uint8_t grandom_hue = 0;
CRGB gdynamic_rgb = 0x000000;
CRGB gdynamic_comp_rgb = 0x000000;

uint8_t gimage_layer_alpha = 255;
bool gdemo_enabled = false;


bool gplaylist_enabled = false;
bool gpattern_layer_enable = true;

struct {
  String type;
  String filename;
} gnextup;

//const char* patterns[] = {"None", "Rainbow", "Solid", "Orbit", "Running Lights", "Juggle", "Sparkle", "Weave", "Checkerboard", "Binary System", "Shooting Star", "Pac-Man", "Cylon", "Demo"};
//const char* accents[] = {"None", "Glitter", "Confetti", "Flicker", "Frozen Decay"};
//const String patterns_json = "{\"patterns\":[\"None\", \"Rainbow\", \"Solid\", \"Orbit\", \"Running Lights\", \"Juggle\", \"Sparkle\", \"Weave\", \"Checkerboard\", \"Binary System\", \"Shooting Star\", \"Pac-Man\", \"Cylon\", \"Demo\"]}";
//const String accents_json = "{\"accents\":[\"None\", \"Glitter\", \"Confetti\", \"Flicker\", \"Frozen Decay\"]}";
const String patterns_json = "[\"Rainbow\", \"Solid\", \"Orbit\", \"Running Lights\", \"Juggle\", \"Sparkle\", \"Weave\", \"Checkerboard\", \"Binary System\", \"Shooting Star\", \"Pac-Man\", \"Cylon\", \"Demo\"]";
const String accents_json = "[\"Glitter\", \"Confetti\", \"Flicker\", \"Frozen Decay\"]";



void create_dirs(String path);
void list_files(File dir, String parent);
void handle_file_list(void);
void delete_files(String name, String parent);
void handle_delete_list(void);
String form_path(String root, String id);

bool save_data(String fs_path, String json, String* message);
bool load_image_to_layer(String json, String* message = nullptr);
bool load_composite_from_json(String json, String* message = nullptr);
bool load_file(String fs_path, String* message = nullptr);
bool load_from_playlist(String id, String* message = nullptr);
void demo(void);
void web_server_initiate(void);



/*
void create_layer(uint8_t type) {

  if (onions[0] != nullptr) {
    delete onions[0];
  }
  onions[0] = new Layer(&gdynamic_rgb);

}
*/


/*
String list_accents() {

}
*/

/*
String list_patterns() {
  DynamicJsonDocument root(4000);
  JsonArray rxh = root.createNestedArray("rx");
  for (char i = 0; i < rxc; i++) {
    rxh.add(rxhex[i]);
  }
  String json;
  serializeJson(root, json);
}
*/

/*
String list_patterns() {
  const size_t CAPACITY = JSON_ARRAY_SIZE(3);
  StaticJsonDocument<CAPACITY> doc;
  deserializeJson(doc, "[1,2,3]");

  JsonArray array = doc.as<JsonArray>();
  for(JsonVariant v : array) {
    Serial.println(v.as<int>());
  }
}
*/






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





bool load_image_to_layer(String fs_path, String* message) {

  for (uint8_t i = 0; i < NUM_LAYERS; i++) {
    if (onions[i] != nullptr) {
      delete onions[i];
      onions[i] = nullptr;
    }
 }

 if (onions[0] == nullptr) {
   onions[0] = new Layer();
   onions[0]->load_image_from_file(fs_path);
 }

  //??? actual success ??? need to set message too
  return true;
}


// Re: delete onions[i]
//This sort of code is brittle because it is not exception-safe: if an exception is thrown between when you create the object and when you delete it, you will leak that object.
//It is far better to use a smart pointer container,
// need to use smart pointers
//use unique_ptr or shared_ptr?
bool load_composite_from_json(String json, String* message) {
  //const size_t CAPACITY = JSON_OBJECT_SIZE(6) + JSON_ARRAY_SIZE(360);
  //StaticJsonDocument<CAPACITY> doc;
  DynamicJsonDocument doc(768);

  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    //Serial.print("deserializeJson() failed: ");
    //Serial.println(error.c_str());
    if (message) {
      *message = F("load_composite_from_json: deserializeJson() failed.");
    }
    return false;
  }

  JsonObject object = doc.as<JsonObject>();

  JsonArray arr = object[F("l")];
  if (!arr.isNull() && arr.size() > 0) {
    for (uint8_t i = 0; i < arr.size(); i++) {
      if(arr[i].is<JsonVariant>()) {
        JsonVariant jlyr = arr[i];
        String type = jlyr[F("t")];
        Serial.print(i);
        Serial.print(" type: ");
        Serial.println(type);

        if (jlyr[F("t")] == "e") {
          if (onions[i] != nullptr) {
            delete onions[i];
            onions[i] = nullptr;
          }
        }
        else if (jlyr[F("t")] == "i") {
          if (onions[i] == nullptr) {
            onions[i] = new Layer();
          }

          // images do not use the layer color variable currently. possibly in the future they will.
          // layer color variable still need to be set to avoid dereferencing a nullptr.
          uint8_t ct = jlyr[F("ct")];
          if (ct == 0) {          
            onions[i]->set_color(&gdynamic_rgb);
          }
          else if (ct == 1) {          
            onions[i]->set_color(&gdynamic_comp_rgb);
          }
          else {
            std::string s = jlyr[F("c")];
            uint32_t fc = std::stoul(s, nullptr, 16);
            onions[i]->set_color(fc);
          }
          onions[i]->load_image_from_file(jlyr[F("id")]);
        }
        else if (jlyr[F("t")] == "p") {
          if(jlyr[F("id")].is<JsonInteger>()) {
            String plfx = jlyr[F("id")];
            Serial.print("pattern id: ");
            Serial.println(plfx);

            if (onions[i] == nullptr) {
              onions[i] = new Layer();
            }

            uint8_t ct = jlyr[F("ct")];
            if (ct == 0) {          
              onions[i]->set_color(&gdynamic_rgb);
            }
            else if (ct == 1) {          
              onions[i]->set_color(&gdynamic_comp_rgb);
            }
            else {
              std::string s = jlyr[F("c")];
              uint32_t fc = std::stoul(s, nullptr, 16);
              onions[i]->set_color(fc);
            }
            onions[i]->set_plfx(jlyr[F("id")]);
          }
        }
        else if (jlyr[F("t")] == "a") {
          if(jlyr[F("id")].is<JsonInteger>()) {
            String alfx = jlyr[F("id")];
            Serial.print("accent id: ");
            Serial.println(alfx);
            if (onions[i] == nullptr) {
              onions[i] = new Layer();
            }

            uint8_t ct = jlyr[F("ct")];
            if (ct == 0) {          
              onions[i]->set_color(&gdynamic_rgb);
            }
            else if (ct == 1) {          
              onions[i]->set_color(&gdynamic_comp_rgb);
            }
            else {
              std::string s = jlyr[F("c")];
              uint32_t fc = std::stoul(s, nullptr, 16);
              onions[i]->set_color(fc);
            }
            onions[i]->set_alfx(jlyr[F("id")]);
          }
        }
        else if (jlyr[F("t")] == "t") {
          if(jlyr[F("id")].is<JsonInteger>()) {
            String s = jlyr[F("id")];
            Serial.print("text id: ");
            Serial.println(s);
            if (onions[i] == nullptr) {
              onions[i] = new Layer();
            }

            uint8_t ct = jlyr[F("ct")];
            if (ct == 0) {          
              onions[i]->set_color(&gdynamic_rgb);
            }
            else if (ct == 1) {          
              onions[i]->set_color(&gdynamic_comp_rgb);
            }
            else {
              std::string s = jlyr[F("c")];
              uint32_t fc = std::stoul(s, nullptr, 16);
              onions[i]->set_color(fc);
            }

            // need to check jlyr[F("id")] // text or time
            onions[i]->set_text(jlyr[F("w")]);
          }
        }
      }
    }
  }
  //??? actual success ??? need to check deserializeSegment()
  return true;
}


bool load_file(String fs_path, String* message) {
  File file = LittleFS.open(fs_path, "r");
  
  if(!file){
    if (message) {
      *message = F("load_file(): File not found.");
    }
    return false;
  }

  if (file.available()) {
    if (fs_path.startsWith(CM_ROOT)) {
      load_composite_from_json(file.readString());
    }
    else if (fs_path.startsWith(IM_ROOT)) {      
      load_image_to_layer(fs_path);
    }
  }
  file.close();

  if (message) {
    *message = F("load_file(): File loaded.");
  }
  return true;
}


bool load_from_playlist(String id = "", String* message) {
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
          *message = "load_from_playlist(): deserializeJson failed.";
        }
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
          *message = "load_from_playlist(): Invalid playlist.";
        }
      }
    }
  }
  return refresh_needed;
}




/*
void demo() {
  const uint8_t PATTERNS_NUM = 21;

  const Pattern patterns[PATTERNS_NUM] = {DYNAMIC_RAINBOW, SOLID, ORBIT, RUNNING_LIGHTS,
                                          JUGGLE, SPARKLE, WEAVE, CHECKERBOARD, BINARY_SYSTEM,
                                          SOLID, SOLID, SOLID, SOLID, DYNAMIC_RAINBOW,
                                          SHOOTING_STAR, //MITOSIS, BUBBLES, MATRIX,
                                          BALLS, CYLON,
                                          //STARSHIP_RACE
                                          //PAC_MAN, // PAC_MAN crashes ???
                                          //SOUND_BLOCKS, SOUND_BLOCKS, SOUND_RIBBONS, SOUND_RIBBONS,
                                          //SOUND_ORBIT, SOUND_ORBIT, SOUND_RIPPLE, SOUND_RIPPLE
                                          };



  const Overlay overlays[PATTERNS_NUM] = {NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY,
                                          NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY,
                                          GLITTER, BREATHING, CONFETTI, FLICKER, FROZEN_DECAY,
                                          NO_OVERLAY, //NO_OVERLAY, NO_OVERLAY, NO_OVERLAY,
                                          NO_OVERLAY, NO_OVERLAY
                                          //NO_OVERLAY
                                          //NO_OVERLAY
                                          //NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY,
                                          //NO_OVERLAY, NO_OVERLAY, NO_OVERLAY, NO_OVERLAY
                                          };

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
*/

void web_server_initiate() {

  web_server.serveStatic("/", LittleFS, "/").setDefaultFile("html/index.html");

  web_server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/index.html");
  });

  web_server.on("/art.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/art.html");
  });

  web_server.on("/compose.html", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(LittleFS, "/html/compose.html");
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

    if (id != "" && (type == "im" || type == "pl")) {
      gnextup.type = type;
      gnextup.filename = id;
    }
    else {
      message = "Invalid type.";
    }

    request->send(200, "application/json", "{\"message\": \""+message+"\"}");
  });

  // memory hog?
  web_server.on("/options.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    String options_json = "{\"files\":"+gfile_list_json + ",\"patterns\":"+patterns_json + ",\"accents\":"+accents_json + "}"; 
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
    onions[i] = nullptr;
  }

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
  }
  gmax_brightness = calculate_max_brightness_for_power_vmA(leds, NUM_LEDS, 255, LED_STRIP_VOLTAGE, LED_STRIP_MILLIAMPS);
  gmax_brightness = 128; // overriding for testing
  FastLED.clear();
  FastLED.setBrightness(gmax_brightness);
  FastLED.show();

  //random16_set_seed(analogRead(A0)); // use randomness ??? need to look up which pin for ESP32 ???

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


  // for testing how layers handle background
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    leds[i] = 0xFF00FF;
  }
  FastLED.show();


  //load_matrix_from_file("/files/am/blinky.json");
  
  //load_from_playlist("/files/pl/tinyrick.json");
  //load_from_playlist("/files/pl/nyan.json");

  //load_matrix_from_file("/files/am/mushroom_hole_.json");

  //load_matrix_from_file("/files/art/bottle_semitransparent.json");

  //load_matrix_from_file("/files/art/test.json");

  load_file("/files/cm/mycomp.json");

  //debug_print_test();

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
    if (gnextup.type == "pl") {
      load_from_playlist(gnextup.filename);
    }
    else if (gnextup.type == "cm") {
      load_file(gnextup.filename);
    }
    gnextup.type = "";
    gnextup.filename = "";
  }

  if (gplaylist_enabled) {
    refresh_now = load_from_playlist();
  }

  //if(gdemo_enabled) {
  //  demo(); // cycles through all of the patterns
  //}

  //if (gpattern_layer_enable && (image_has_transparency || gimage_layer_alpha != 255)) {
  if (true) {
    // draw layers. changes here are not drawn until they are copied to leds[] when the refresh_block
    for (uint8_t i = 0; i < NUM_LAYERS; i++) {
      if (onions[i] != nullptr) {
        onions[i]->run();
      }
    }
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

/*
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

      //matrix_text("Hello world! ");
      //String s = "Hello world!";
      ftext.s = "Hello world!";
      ftext.vmargin = (MD-get_text_height(ftext.s))/2;
      matrix_text_shift(ftext);
      leds[i] = nblend(leds[i], tl[i], 224);
    }
    // calling FastLED.show(); at the bottom of loop() instead of here gives smoother transitions
  }
*/

  if((millis()-pm) > 100 || refresh_now) {
    pm = millis();  // worked without this ???
    refresh_now = false;
    image_has_transparency = false;
    gdynamic_hue+=3;
    gdynamic_rgb = CHSV(gdynamic_hue, 255, 255);
    gdynamic_comp_rgb = CRGB::White - gdynamic_rgb;
    //gdynamic_hue = 128;
    //gdynamic_hue+=1;
    grandom_hue = random8();


CRGBA pixel;
    //FastLED.clear(); // background has an interesting blurry fade effect without this.
    bool is_bg_layer = true;
    for (uint8_t i = 0; i < NUM_LAYERS; i++) {
      if (onions[i] != nullptr) {
        for (uint16_t j = 0; j < NUM_LEDS; j++) {
          // transparency is used in two different ways here.
          // 1) if the pixels of the image have an alpha less than 100% (255) then they will be replaced with the background layer.
          // 2) Pixel Art Convertor has an Alpha slider which is separate from the image's alpha channel. This slider controls how much of the
          //    non-transparent parts of the original image are blended with the layers below it.
          // in short 1) local/pixel level effect and 2) global effect
          //

          image_has_transparency = true; // for testing
          pixel = onions[i]->get_pixel(j);

          //uint8_t pixel_alpha = pixel.a;
          //if (pixel == COLORSUB) {
          //  pixel = CHSV(gdynamic_hue+96, 255, 255); // CHSV overwrites the alpha value
          //  pixel.a = pixel_alpha;
          //}

          // treat the first non-empty layer we touch as the background layer and give it zero transparency to completely overwrite the previous frame.
          if (is_bg_layer) {
            pixel.a = 255;
          }
          leds[j] = nblend(leds[j], (CRGB)pixel, pixel.a);
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
        is_bg_layer = false;
      }
    }
    //Serial.println("----");
  }

  FastLED.show(); // what is the best place to call show() ??? call this less frequently ???

  handle_file_list();
  handle_delete_list();
}