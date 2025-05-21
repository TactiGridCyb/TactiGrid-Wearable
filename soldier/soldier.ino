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
#include "../wifi-connection/WifiModule.h"

std::unique_ptr<LoraModule> loraModule;
std::unique_ptr<WifiModule> wifiModule;

const char* ssid = "default";
const char* password = "1357924680";

const char *udpAddress = "192.168.0.44";
const int udpPort = 3333;

#include "../LoraModule/FHFModule.h"

const std::vector<uint32_t> freqList = {
    433500000, 434000000, 434500000, 435000000
};
const uint32_t hopIntervalSeconds = 10;

std::unique_ptr<FHFModule> fhfModule;

float currentLoraFreq = 0.0f;

const crypto::Key256 SHARED_KEY = []() {
  crypto::Key256 key{};
  const char* raw = "0123456789abcdef0123456789abcdef"; 
  std::memcpy(key.data(), raw, 32);
  return key;
}();

//Example soldiers messages
SoldiersSentData coords[] = {
  {0, 0, 31.970866, 34.785664,  78, 2},
  {0, 0, 31.970870, 34.785683, 100, 2},
  {0, 0, 31.970855, 34.785643,  55, 2}, 
  {0, 0, 31.970840, 34.785623,   0, 2},
  {0, 0, 31.970880, 34.785703, 120, 2}
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


  loraModule = std::make_unique<LoraModule>(433.5);
  loraModule->setup(true);

  wifiModule = std::make_unique<WifiModule>(ssid, password);
  wifiModule->connect(60000);
  Serial.printf("WiFi connected!");

  fhfModule = std::make_unique<FHFModule>(freqList, hopIntervalSeconds);
  currentLoraFreq = 433.5;

  struct tm timeInfo;
  Serial.println("HELLO WORLD!");
  setenv("TZ", "GMT-3", 1);
  tzset();
  deleteExistingFile("/cert.pem");
// Create cert.pem file with certificate content if it does not exist
  if (!FFat.exists("/cert.pem")) {
    Serial.println("cert file not exists!");
    File certFile = FFat.open("/cert.pem", FILE_WRITE);
    if (certFile) {
      Serial.println("cert file opened!");
      const char* certContent = R"(-----BEGIN CERTIFICATE-----
MIID1zCCAb+gAwIBAgIHEOPaLFZerzANBgkqhkiG9w0BAQsFADBFMQswCQYDVQQG
EwJJTDETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50ZXJuZXQgV2lk
Z2l0cyBQdHkgTHRkMB4XDTI1MDUwOTEwMjAzOVoXDTI2MDUwOTEwMjAzOVowEzER
MA8GA1UEAxMIc29sZGllcjEwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIB
AQC0oQS/0YaIPw8jJ0b3Y26H3Y0raWKB5LUqGz9fooIaMUTyD1ouJ0bKzh5vhPOo
+igTlU+MF+USt4Ey2R3O15fwYKmlzpbPwEkDGEXsA+SJQlPooXHjxbgAsB87rPil
fvCOdCkbPaNYl14sfbjGJFTfPCDPiPBGCJJ/YU9hFoQDRo0OetOZ8rkXhoyukywT
u6UPaTgo0w/SRaw3PV2Ea3du/+XrcAYL5lcyhYXUbPUWU8vk2zCSlxkaKeW3bSSw
8Ms9A18/1teasRvWu5IEoIj8e258Ah5KAO2pxDEvCuC1Pv0+0fp3GhEGQ34Yjbd9
DZ21B7jKLLcLqo1xmsa4pQ+BAgMBAAEwDQYJKoZIhvcNAQELBQADggIBAF9vpL3F
cwwJz3vZTQkJjuAiY8HdiKJ3iRKC/y853W8ZpmCwLLphSC5VD7vMIt+VlYuf5gNM
Ms6eohzXlAVeNsaQJ3KW+5PI6Q/9SmqcFes/qpwQ39/EvkLpPskPSEqqEWaoSOd0
60+KrRT5u4DG3dgWtWW5i53709dv2wSXDCK3PWzkptvxugA2+2wZwrdfnVeLH8fb
2bp3/61Bn7aavCLC/NadmqGaPWA3AuXJFll+u6sQD46WK4ChH94d5cx88pxji6sM
s5Ki2DcCDrpG37YsXbF4/gUvJL5Z8vIn5swXZSOnZevBMDddLH8xT7RTE13q+ZYB
Q+695pvpl33QXLSFhGRZPA3K2Kpn87pEPJNou0JkkGxP+jYk8uYpbWlB1tfnC3dt
nR6dFo4iKY9WFc6LHDxtYzf+nThoz9o74NTpY790OUcxbt1d6oaK+15ob+RghSW6
Fj2Jb91Vsm96HozE8B3Xn+XwBB2jzrAqYWs5VvFqsm2NchVy7zZ+L1Z4eh3DRqUQ
ErXSoivNVFfTDhTKpuGQs4qF6NCYUWFn5LO4QyHuuorlyLZXhnsHOxlZIktVRgkr
GLLseRj4Yp23+4z4o3hpEavecLgFIhuT9yBMBvxbDm9G3KLvbYYEaFOx1ItLLVnn
WnuAIqZGA5XJrhEujtZqR7EecTPgYH2pD8d8
-----END CERTIFICATE-----
)";
      certFile.print(certContent);
      certFile.close();
    }
  }
  else
  {
    Serial.println("Cert file exists!");
  }

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

void init_send_large_log_file_page(lv_event_t * event)
{
  Serial.println("init_send_large_log_file_page");
  File file = FFat.open("/cert.pem", FILE_READ);
  if (file)
  {
    size_t size = file.size();
    uint8_t* buff = static_cast<uint8_t*>(malloc(size));
    Serial.printf("%d %d", size, sizeof(buff));
    if (!buff)
    {
      Serial.println("NO BUFF FOUND");
      return;
    }

    size_t n = file.read(buff, size);
    file.close();
    Serial.printf("BUFF IS: %s\n", (char*)buff);
    loraModule->sendFile(buff, n);

  }
  else
  {
    Serial.println("NO FILE FOUND");
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

  sendTimer = lv_timer_create(sendTimerCallback, 5000, NULL);
  //startGPSChecking = true;

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
    
    sendCoordinate(coords[currentIndex % coordCount].posLat, coords[currentIndex % coordCount].posLon,
       coords[currentIndex % coordCount].heartRate, coords[currentIndex % coordCount].soldiersID);
    
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

  if (fhfModule) {
      uint32_t newFreqHz = fhfModule->currentFrequency();
      float newFreqMHz = newFreqHz / 1e6f;
      if (newFreqMHz != currentLoraFreq) {
          currentLoraFreq = newFreqMHz;
          loraModule->setFrequency(currentLoraFreq);
          Serial.printf("Frequency hopped to %.3f MHz\n", currentLoraFreq);
      }
  }

  if (startGPSChecking) {
    gpsModule->readGPSData();

    gpsModule->updateCoords();
    unsigned long currentTime = millis();

    float lat = gpsModule->getLat();
    float lon = gpsModule->getLon();

    if (currentTime - lastSendTime >= SEND_INTERVAL) {

      if (!isZero(lat) && !isZero(lon)) {
        sendCoordinate(lat, lon, 100, 1);
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


void sendCoordinate(float lat, float lon, uint16_t heartRate, uint16_t soldiersID) {
  Serial.println("sendCoordinate");

  SoldiersSentData coord;
  coord.posLat = lat;
  coord.posLon = lon;
  coord.heartRate = heartRate;
  coord.soldiersID = soldiersID;

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
