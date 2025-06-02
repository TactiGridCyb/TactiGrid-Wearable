#include "SoldiersMissionPage.h"
#include <Commander.h>
#include <cstring>
#include <algorithm>

inline bool SoldiersMissionPage::isZero(float x)
{
    return std::fabs(x) < 1e-9f;
}

SoldiersMissionPage::SoldiersMissionPage(std::shared_ptr<LoraModule> loraModule,
     std::unique_ptr<WifiModule> wifiModule, std::shared_ptr<GPSModule> gpsModule, std::unique_ptr<FHFModule> fhfModule, 
     std::unique_ptr<Soldier> soldierModule, bool fakeGPS)
{
    this->loraModule = std::move(loraModule);
    this->wifiModule = std::move(wifiModule);
    this->gpsModule = std::move(gpsModule);
    this->fhfModule = std::move(fhfModule);
    this->soldierModule = std::move(soldierModule);

    this->loraModule->setOnFileReceived([this](const uint8_t* data, size_t len) {
        this->onDataReceived(data, len);
    });

    this->loraModule->setOnReadData([this](const uint8_t* data, size_t len) {
        this->loraModule->onLoraFileDataReceived(data, len);
    });

    this->mainPage = lv_scr_act();

    this->fakeGPS = fakeGPS;
    this->currentIndex = 0;
    Serial.println(true ? this->wifiModule->isConnected() : false);

    this->coordCount = 5;
}

void SoldiersMissionPage::createPage()
{
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "TactiGrid Soldier");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);


    this->sendLabel = lv_label_create(mainPage);
    lv_obj_align(this->sendLabel, LV_ALIGN_TOP_MID, 0, 15);
    lv_obj_set_style_text_color(this->sendLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(this->sendLabel, "");

    lv_obj_set_width(this->sendLabel, lv_disp_get_hor_res(NULL) - 20);
    lv_label_set_long_mode(this->sendLabel, LV_LABEL_LONG_WRAP);

    if(this->fakeGPS)
    {
        lv_timer_t * sendTimer = lv_timer_create(SoldiersMissionPage::sendTimerCallback, 7000, this);
    }
    else
    {

    }

    lv_timer_create([](lv_timer_t* t){
        auto *self = static_cast<SoldiersMissionPage*>(t->user_data);
        self->loraModule->handleCompletedOperation();

        if (!self->loraModule->isBusy()) {
            self->loraModule->readData();
        }

        self->loraModule->syncFrequency(self->fhfModule.get());
    }, 50, this);
    
}

void SoldiersMissionPage::onDataReceived(const uint8_t* data, size_t len)
{
    if (len < 4) {
        Serial.println("Received data too short for SwitchGMK with length prefix");
        return;
    }

    std::string base64Str(reinterpret_cast<const char*>(data), len);
    Serial.printf("DECODED DATA: %s\n", base64Str.c_str());

    std::vector<uint8_t> decodedData;
    Serial.println("-----------------------------");

    try {
        decodedData = crypto::CryptoModule::base64Decode(base64Str);
        
    } catch (const std::exception& e) {
        Serial.printf("Base64 decode failed: %s\n", e.what());
        return;
    }

    if (decodedData.size() < 2) {
        Serial.println("Decoded data too short for SwitchGMK with length prefix");
        return;
    }

    Serial.printf("PAYLOAD LEN: %d\n", base64Str.length());

    SwitchGMK payload;
    payload.msgID = static_cast<uint8_t>(decodedData[0]);
    payload.soldiersID = static_cast<uint8_t>(decodedData[1]);

    if(payload.soldiersID != this->soldierModule->getSoldierNumber())
    {
        return;
    }

    payload.encryptedGMK.assign(decodedData.begin() + 2, decodedData.end());
    
    this->onGMKSwitchEvent(payload);

    Serial.printf("Deserialized SwitchGMK: msgID=%d, soldiersID=%d, encryptedGMK size=%zu\n",
                  payload.msgID, payload.soldiersID, payload.encryptedGMK.size());

}

void SoldiersMissionPage::onGMKSwitchEvent(SwitchGMK payload)
{
    std::string decryptedStr;
    bool success = certModule::decryptWithPrivateKey(this->soldierModule->getPrivateKey(), payload.encryptedGMK, decryptedStr);
    if (!success) {
        Serial.println("Decryption failed in onGMKSwitchEvent");
        return;
    }
    Serial.println("ENCRYPTED GMK: ");
    Serial.printf("NEW GMK SET: %s\n", decryptedStr.c_str());
    crypto::Key256 newGMK = crypto::CryptoModule::strToKey256(decryptedStr);
    this->soldierModule->setGMK(newGMK);

    // Delay the next send in 10 seconds
}

void SoldiersMissionPage::sendCoordinate(float lat, float lon, uint16_t heartRate, uint16_t soldiersID) {
  Serial.println("sendCoordinate");
  
  this->loraModule->switchToTransmitterMode();

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
  
  crypto::Ciphertext ct = crypto::CryptoModule::encrypt(this->soldierModule->getGMK(), payload);

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

  this->loraModule->cancelReceive();
  
  int16_t transmissionState = loraModule->sendData(msg.c_str());

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

void SoldiersMissionPage::sendTimerCallback(lv_timer_t *timer) {
    auto* self = static_cast<SoldiersMissionPage*>(timer->user_data);
    Serial.println("sendTimerCallback");
    Serial.println(self->currentIndex);

    if(self->currentIndex < 20) {

        struct tm timeInfo;
        char timeStr[9];
        Serial.println("timeStr");
        if(getLocalTime(&timeInfo)) {
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeInfo);
        } else {
            strcpy(timeStr, "00:00:00");
        }

        Serial.println(timeStr);
        Serial.println("getLocalTime");
        self->sendCoordinate(self->coords[self->currentIndex % self->coordCount].posLat, self->coords[self->currentIndex % self->coordCount].posLon,
        self->coords[self->currentIndex % self->coordCount].heartRate, self->coords[self->currentIndex % self->coordCount].soldiersID);
        
        const char *current_text = lv_label_get_text(self->sendLabel);
        
        lv_label_set_text_fmt(self->sendLabel, "%s%s - sent coords {%.5f, %.5f}\n", 
                            current_text, timeStr, 
                            self->coords[self->currentIndex % self->coordCount].posLat, 
                            self->coords[self->currentIndex % self->coordCount].posLon);
                            
        self->currentIndex++;
    } else {
        lv_timer_del(timer);
        self->currentIndex = 0;
    }
}


std::pair<float, float> SoldiersMissionPage::getTileCenterLatLon(float lat, float lon, int zoomLevel, float tileSize) {

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

void SoldiersMissionPage::parseGPSData()
{
    if(this->fakeGPS)
    {
        return;
    }

    float lat = this->gpsModule->getLat();
    float lon = this->gpsModule->getLon();

    if (!isZero(lat) && !isZero(lon)) {
        sendCoordinate(lat, lon, 100, 1);
    }
    else
    {
        TinyGPSSpeed gpsSpeed = this->gpsModule->getGPSSpeed();
        TinyGPSInteger gpsSatellites = this->gpsModule->getGPSSatellites();
        TinyGPSAltitude gpsAltitude = this->gpsModule->getGPSAltitude();
        TinyGPSHDOP gpsHDOP = this->gpsModule->getGPSHDOP();
        TinyGPSTime gpsTime = this->gpsModule->getGPSTime();
        TinyGPSDate gpsDate = this->gpsModule->getGPSDate();

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
        lv_label_set_text_fmt(this->sendLabel, "Sats:%u\nHDOP:%.1f\nLat:%.5f\nLon:%.5f\nDate:%d/%d/%d \nTime:%d/%d/%d\nAlt:%.2f m \nSpeed:%.2f",
                            satellites, hdop, lat, lon, year, month, day, hour, minute, second, meters, kmph);
    }
}
