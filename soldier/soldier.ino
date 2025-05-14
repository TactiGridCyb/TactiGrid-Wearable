#include <cstring>
#include <WiFiUdp.h> 
#include <WiFi.h>
#include <HTTPClient.h>
#include <LV_Helper.h>
#include <LilyGoLib.h>
#include "../encryption/CryptoModule.h"
#include "../gps-collect/GPSModule.h"
#include "SoldiersSentData.h"
#include "LoraModule.h"

std::unique_ptr<LoraModule> loraModule;

const char* ssid = "default";
const char* password = "1357924680";

const char *udpAddress = "192.168.0.44";
const int udpPort = 3333;

const crypto::Key256 SHARED_KEY = []() {
  crypto::Key256 key{};
  const char* raw = "0123456789abcdef0123456789abcdef"; 
  std::memcpy(key.data(), raw, 32);
  return key;
}();

//Example soldiers messages
SoldiersSentData coords[] = {
  {0, 0, 31.970866, 34.785611, 78, 1},     // healthy
  {0, 0, 31.970870, 34.785630, 100, 2},    // borderline
  {0, 0, 31.970855, 34.785590, 55, 3},     // low
  {0, 0, 31.970840, 34.785570, 0, 4},      // dead 
  {0, 0, 31.970880, 34.785650, 120, 5}     // high
};

const int coordCount = sizeof(coords) / sizeof(coords[0]);
int currentIndex = 0;
bool alreadyTouched = false;
int transmissionState = RADIOLIB_ERR_NONE;
lv_obj_t* sendLabel = NULL;
lv_timer_t * sendTimer;
WiFiUDP udp;
std::unique_ptr<GPSModule> gpsModule;
const char* helmentDownloadLink = "https://iconduck.com/api/v2/vectors/vctr0xb8urkk/media/png/256/download";

// === Function Prototypes ===
void setupLoRa();
void sendCoordinate(float lat, float lon);
bool screenTouched();

volatile bool pmu_flag = false;
volatile bool startGPSChecking = false;

unsigned long lastSendTime = 0;
const unsigned long SEND_INTERVAL = 5000;

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

  Serial.println("Touch screen to send next coordinate.");
  if(!FFat.exists("/helmet.png"))
  {
    
    downloadFile(helmentDownloadLink, "/helmet.png");
  }
  connectToWiFi();


  loraModule = std::make_unique<LoraModule>(433.5);
  loraModule->setup(true);

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
  gpsModule = std::make_unique<GPSModule>();
  
  init_main_poc_page();
}

std::pair<float, float> getTileCenterLatLon(float lat, float lon, int zoomLevel, float tileSize) {

  double lat_rad = lat * M_PI / 180.0;
  double n = std::pow(2.0, zoomLevel);

  int x_tile = static_cast<int>(std::floor((lon + 180.0) / 360.0 * n));
  int y_tile = static_cast<int>(std::floor((1.0 - std::log(std::tan(lat_rad) + 1.0 / std::cos(lat_rad)) / M_PI) / 2.0 * n));

  int centerX = x_tile * tileSize + tileSize / 2;
  int centerY = y_tile * tileSize + tileSize / 2;

  double lon_deg = centerX / (n * tileSize) * 360.0 - 180.0;
  double lat_rad_center = std::atan(std::sinh(M_PI * (1 - 2.0 * centerY / (n * tileSize))));
  double lat_deg = lat_rad_center * 180.0 / M_PI;

  return {static_cast<float>(lat_deg), static_cast<float>(lon_deg)};
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

    startGPSChecking = false;

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

  //sendTimer = lv_timer_create(sendTimerCallback, 5000, NULL);
  startGPSChecking = true;

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
    
    //sendCoordinate(currentIndex % coordCount);
    
    const char *current_text = lv_label_get_text(sendLabel);
    
    lv_label_set_text_fmt(sendLabel, "%s%s - sent coords {%.5f, %.5f}\n", 
                          current_text, timeStr, 
                          coords[currentIndex % coordCount].posLat, 
                          coords[currentIndex % coordCount].posLon);
                          
    currentIndex++;
  } else {
      lv_timer_del(timer);
      currentIndex = 0;
  }
}

bool isZero(float x)
{
    return std::fabs(x) < 1e-9f;
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

  loraModule->cleanUpTransmissions();

  if (startGPSChecking) {
    gpsModule->readGPSData();

    gpsModule->updateCoords();
    unsigned long currentTime = millis();

    float lat = gpsModule->getLat();
    float lon = gpsModule->getLon();

    if (currentTime - lastSendTime >= SEND_INTERVAL) {

      if (!isZero(lat) && !isZero(lon)) {
        sendCoordinate(lat, lon);
      }
      else
      {
        TinyGPSSpeed gpsSpeed = gpsModule->getGPSSpeed();
        TinyGPSInteger gpsSatellites = gpsModule->getGPSSatellites();
        TinyGPSAltitude gpsAltitude = gpsModule->getGPSAltitude();
        TinyGPSHDOP gpsHDOP = gpsModule->getGPSHDOP();
        TinyGPSTime gpsTime = gpsModule->getGPSTime();
        TinyGPSDate gpsDate = gpsModule->getGPSDate();

        uint32_t  satellites = gpsSatellites.isValid() ? gpsSatellites.value() : 0;
        double hdop = gpsHDOP.isValid() ? gpsHDOP.hdop() : 0;
        uint16_t year = gpsDate.isValid() ? gpsDate.year() : 0;
        uint8_t  month = gpsDate.isValid() ? gpsDate.month() : 0;
        uint8_t  day = gpsDate.isValid() ? gpsDate.day() : 0;
        uint8_t  hour = gpsTime.isValid() ? gpsTime.hour() : 0;
        uint8_t  minute = gpsTime.isValid() ? gpsTime.minute() : 0;
        uint8_t  second = gpsTime.isValid() ? gpsTime.second() : 0;
        double  meters = gpsAltitude.isValid() ? gpsAltitude.meters() : 0;
        double  kmph = gpsSpeed.isValid() ? gpsSpeed.kmph() : 0;
        lv_label_set_text_fmt(sendLabel, "Sats:%u\nHDOP:%.1f\nLat:%.5f\nLon:%.5f\nDate:%d/%d/%d \nTime:%d/%d/%d\nAlt:%.2f m \nSpeed:%.2f",
          satellites, hdop, lat, lon, year, month, day, hour, minute, second, meters, kmph);
      }

      lastSendTime = currentTime;
    }

  }
  
  lv_task_handler();
  delay(10);
}


void sendCoordinate(float lat, float lon) {
  Serial.println("sendCoordinate");

  SoldiersSentData coord;
  coord.posLat = lat;
  coord.posLon = lon;

  Serial.printf("BEFORE SENDING: %.7f %.7f %.7f %.7f %d\n",coord.tileLat, coord.tileLon, coord.posLat, coord.posLon, coord.heartRate);
  auto [tileLat, tileLon] = getTileCenterLatLon(coord.posLat, coord.posLon, 19, 256);

  coord.tileLat = tileLat;
  coord.tileLon = tileLon;
  Serial.printf("SENDING: %.7f %.7f %.7f %.7f %d\n",coord.tileLat, coord.tileLon, coord.posLat, coord.posLon, coord.heartRate);
  crypto::ByteVec payload;
  payload.resize(sizeof(SoldiersSentData));
  std::memcpy(payload.data(), &coord, sizeof(SoldiersSentData));
  
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
  transmissionState = loraModule->sendData(msg.c_str());

  struct tm timeInfo;
  char timeStr[9];

  if(getLocalTime(&timeInfo)) {
      strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeInfo);
  } else {
      strcpy(timeStr, "00:00:00");
  }
        
  lv_label_set_text_fmt(sendLabel, "%s - sent coords {%.5f, %.5f}\n", 
                        timeStr,
                        lat, 
                        lon);
}
