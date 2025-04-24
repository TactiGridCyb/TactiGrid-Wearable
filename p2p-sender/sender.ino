#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <WiFiUdp.h> 
#include <WiFi.h>
#include <HTTPClient.h>
#include "../encryption/CryptoModule.h"
#include <cstring>

// === LoRa Setup ===
SX1262 lora = newModule();

const char* ssid = "default";
const char* password = "1357924680";

const char *udpAddress = "192.168.0.133";
const int udpPort = 3333;

const crypto::Key256 SHARED_KEY = []() {
  crypto::Key256 key{};
  const char* raw = "0123456789abcdef0123456789abcdef"; 
  std::memcpy(key.data(), raw, 32);
  return key;
}();

// === Coordinates ===
struct GPSCoord {
  float lat1;
  float lon1;
  float lat2;
  float lon2;
};

GPSCoord coords[] = {
  {32.08530, 34.78180, 32.08539,   34.78180},
  {32.08530, 34.78180, 32.0853278, 34.7819010},
  {32.08530, 34.78180, 32.0852272, 34.7818623},
  {32.08530, 34.78180, 32.0852272, 34.7817377},
  {32.08530, 34.78180, 32.0853278, 34.7816990}
};

const int coordCount = sizeof(coords) / sizeof(coords[0]);
int currentIndex = 0;
bool alreadyTouched = false;
int transmissionState = RADIOLIB_ERR_NONE;
lv_obj_t* sendLabel = NULL;
lv_timer_t * sendTimer;
WiFiUDP udp;
const char* helmentDownloadLink = "https://iconduck.com/api/v2/vectors/vctr0xb8urkk/media/png/256/download";

// === Function Prototypes ===
void setupLoRa();
void sendCoordinate(int index);
bool screenTouched();

volatile bool transmittedFlag = false;
volatile bool pmu_flag = false;

ICACHE_RAM_ATTR void setFlag(void)
{
    // we sent a packet, set the flag
    transmittedFlag = true;
}

void IRAM_ATTR onPmuInterrupt() {
  pmu_flag = true;
}

bool saveTileToFFat(const uint8_t* data, size_t len, const char* tileFilePath) {
  File file = FFat.open(tileFilePath, FILE_WRITE);
  if (!file) {
      return false;
  }
  file.write(data, len);
  file.close();
  return true;
}

bool downloadFile(const std::string &tileURL, const char* fileName) {
  HTTPClient http;
  http.begin(tileURL.c_str());
  http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                               "AppleWebKit/537.36 (KHTML, like Gecko) "
                               "Chrome/134.0.0.0 Safari/537.36");
  http.addHeader("Referer", "https://www.iconduck.com/");
  http.addHeader("Accept", "image/png");

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
      size_t fileSize = http.getSize();
      WiFiClient* stream = http.getStreamPtr();
      uint8_t* buffer = new uint8_t[fileSize];
      stream->readBytes(buffer, fileSize);
      bool success = saveTileToFFat(buffer, fileSize, fileName);
      delete[] buffer;
      http.end();
      return success;
  } else {
      http.end();
      return false;
  }
}

void connectToWiFi() {
  // Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      // Serial.print(".");
  }

  udp.begin(WiFi.localIP(), udpPort);
  Serial.println("\nConnected to WiFi!");
}

void showHelment() {
  lv_obj_t * helmetIMG = lv_img_create(lv_scr_act());

  lv_img_set_src(helmetIMG, "A:/helmet.png");
  lv_obj_align(helmetIMG, LV_ALIGN_BOTTOM_MID, 0, 70);

  lv_img_set_zoom(helmetIMG, 64);

  Serial.println("Showing middle tile");
}

// === Setup ===
void setup() {
  Serial.begin(115200);

  watch.begin(&Serial);
  beginLvglHelper();

  setupLoRa();

  Serial.println("Touch screen to send next coordinate.");
  if(!FFat.exists("/helmet.png"))
  {
    
    downloadFile(helmentDownloadLink, "/helmet.png");
  }
  connectToWiFi();

  struct tm timeInfo;

  setenv("TZ", "GMT-3", 1);
  tzset();

  configTime(0, 0, "pool.ntp.org");
  while (!getLocalTime(&timeInfo)) {
      Serial.println("Waiting for time sync...");
      delay(500);
  }

  if (getLocalTime(&timeInfo)) {
      char timeStr[20];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);
      Serial.println(timeStr);
  }
  crypto::CryptoModule::init();
  listFiles("/");

  watch.attachPMU(onPmuInterrupt);
  watch.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  watch.clearPMU();
  watch.enableIRQ(
      XPOWERS_AXP2101_PKEY_SHORT_IRQ | 
      XPOWERS_AXP2101_PKEY_LONG_IRQ
  );

  init_main_poc_page();
}

void clearMainPage()
{
    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_remove_style_all(mainPage);
    lv_obj_clean(mainPage);

    sendLabel = NULL;

}

void deleteExistingFile(const char* tileFilePath) {
  if (FFat.exists(tileFilePath)) {
      Serial.println("Deleting existing tile...");
      FFat.remove(tileFilePath);
  }
}

void listFiles(const char* dirname) {

  File root = FFat.open(dirname);
  if (!root) {
      return;
  }
  if (!root.isDirectory()) {
      return;
  }

  File file = root.openNextFile();
  while (file) {
      if (file.isDirectory()) {
      } else {
      }
      file = root.openNextFile();
  }
}


void init_main_poc_page()
{
    Serial.println("init_main_poc_page");
    clearMainPage();

    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    showHelment();

    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "TactiGrid Soldier");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *sendCoordsBtn = lv_btn_create(mainPage);
    lv_obj_center(sendCoordsBtn);
    lv_obj_set_style_bg_color(sendCoordsBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(sendCoordsBtn, init_send_coords_page, LV_EVENT_CLICKED, NULL);

    lv_obj_t *sendCoordsLabel = lv_label_create(sendCoordsBtn);
    lv_label_set_text(sendCoordsLabel, "Send Coords");
    lv_obj_center(sendCoordsLabel);

}


void init_send_coords_page(lv_event_t * event)
{
  Serial.println("init_send_coords_page");
  clearMainPage();

  lv_obj_t * mainPage = lv_scr_act();
  lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *mainLabel = lv_label_create(mainPage);
  lv_label_set_text(mainLabel, "TactiGrid Soldier");
  lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);


  sendLabel = lv_label_create(mainPage);
  lv_obj_align(sendLabel, LV_ALIGN_TOP_MID, 0, 15);
  lv_obj_set_style_text_color(sendLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(sendLabel, "");

  lv_obj_set_width(sendLabel, lv_disp_get_hor_res(NULL) - 20);
  lv_label_set_long_mode(sendLabel, LV_LABEL_LONG_WRAP);

  sendTimer = lv_timer_create(sendTimerCallback, 1500, NULL);

}

void sendTimerCallback(lv_timer_t *timer) {
  if(currentIndex < 9) {

    struct tm timeInfo;
    char timeStr[9];

    if(getLocalTime(&timeInfo)) {
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeInfo);
    } else {
        strcpy(timeStr, "00:00:00");
    }
    
    sendCoordinate(currentIndex % coordCount);
    
    const char *current_text = lv_label_get_text(sendLabel);
    
    lv_label_set_text_fmt(sendLabel, "%s%s - sent coords {%.5f, %.5f}\n", 
                          current_text, timeStr, 
                          coords[currentIndex % coordCount].lat2, 
                          coords[currentIndex % coordCount].lon2);
                          
    currentIndex++;
  } else {
      lv_timer_del(timer);
      currentIndex = 0;
  }
}

// === Loop ===
void loop() {
   
  if (pmu_flag) {
    pmu_flag = false;
    uint32_t status = watch.readPMU();
    if (watch.isPekeyShortPressIrq()) {
        init_main_poc_page();
    }
    watch.clearPMU();
  }

  if (transmittedFlag) {
    transmittedFlag = false;

    if (transmissionState == RADIOLIB_ERR_NONE) {
      Serial.println(F("transmission finished!"));
    } else {
      // Serial.print(F("failed, code "));
      Serial.println(transmissionState);
    }

    lora.finishTransmit();

  }

  // if (screenTouched()) {
  //   sendCoordinate(currentIndex);

  //   ++currentIndex;

  //   if (currentIndex >= coordCount)
  //   {
  //     currentIndex = 0;
  //   }
  // }

  lv_task_handler();
  delay(10);
}

// === LoRa Setup ===
void setupLoRa() {
  // Serial.print(F("[LoRa] Initializing... "));

  int state = lora.begin();

  if (state == RADIOLIB_ERR_NONE) {
      Serial.println(F("success!"));
  } else {
      // Serial.print(F("failed, code "));
      Serial.println(state);
      while (true);
  }


  if (lora.setFrequency(433.5) == RADIOLIB_ERR_INVALID_FREQUENCY) {
    Serial.println(F("Selected frequency is invalid for this module!"));
    while (true);
  }

  lora.setDio1Action(setFlag);

  // Serial.print(F("[SX1262] Sending first packet ... "));
}

// === Touch Detection ===
bool screenTouched() {
  lv_point_t point; //define a point for screen (x,y)
  lv_indev_t *indev = lv_indev_get_next(NULL); //retrive last place touched on screen
  if (!indev) return false;

#if LV_VERSION_CHECK(9,0,0)
  bool isPressed = lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED;
#else
  bool isPressed = indev->proc.state == LV_INDEV_STATE_PRESSED;
#endif

  if (isPressed && !alreadyTouched) {
    alreadyTouched = true;
    lv_indev_get_point(indev, &point);
    // Serial.print("📍 Touch detected at x=");
    // Serial.print(point.x);
    // Serial.print(" y=");
    Serial.println(point.y);
    return true;
  }

  if (!isPressed) {
    alreadyTouched = false;
  }

  return false;
}


void sendCoordinate(int index) {
  Serial.printf("sendCoordinate");
  GPSCoord coord = coords[index];
  crypto::ByteVec payload;
  payload.resize(sizeof(GPSCoord));
  std::memcpy(payload.data(), &coord, sizeof(GPSCoord));
  
  crypto::Ciphertext ct = crypto::CryptoModule::encrypt(SHARED_KEY, payload);

  auto appendHex = [](String& s, uint8_t b) {
    if (b < 0x10) s += "0";
    s += String(b, HEX);
  };

  String msg;
  for (auto b : ct.nonce) appendHex(msg, b);
  msg += "|";
  for (auto b : ct.data)  appendHex(msg, b);
  msg += "|";
  for (auto b : ct.tag)   appendHex(msg, b);

  udp.beginPacket(udpAddress, udpPort);
  udp.printf(msg.c_str());
  udp.endPacket();
  transmissionState = lora.startTransmit(msg.c_str());
}
