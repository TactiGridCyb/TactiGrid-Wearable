#include <SoldierSendCoordsPage.h>

inline bool SoldierSendCoordsPage::isZero(float x)
{
    return std::fabs(x) < 1e-9f;
}

SoldierSendCoordsPage::SoldierSendCoordsPage(std::shared_ptr<LoraModule> loraModule,
     std::unique_ptr<WifiModule> wifiModule, std::shared_ptr<GPSModule> gpsModule, bool fakeGPS)
{
    this->loraModule = std::move(loraModule);
    this->wifiModule = std::move(wifiModule);
    this->gpsModule = std::move(gpsModule);

    this->mainPage = lv_scr_act();

    this->sharedKey = []() {
        crypto::Key256 key{};
        const char* raw = "0123456789abcdef0123456789abcdef"; 
        std::memcpy(key.data(), raw, 32);
        return key;
    }();

    this->fakeGPS = fakeGPS;
    this->currentIndex = 0;
    Serial.println(true ? this->wifiModule->isConnected() : false);

    this->coordCount = 5;
}

void SoldierSendCoordsPage::createPage()
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
        lv_timer_t * sendTimer = lv_timer_create(SoldierSendCoordsPage::sendTimerCallback, 7000, this);
    }
    else
    {

    }

    lv_timer_create([](lv_timer_t* t){
        auto *self = static_cast<SoldierSendCoordsPage*>(t->user_data);
        self->loraModule->handleCompletedOperation();

        if (!self->loraModule->isBusy()) {
            self->loraModule->readData();
        }
    }, 100, this);
    
}

void SoldierSendCoordsPage::sendCoordinate(float lat, float lon, uint16_t heartRate, uint16_t soldiersID) {
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
  
  crypto::Ciphertext ct = crypto::CryptoModule::encrypt(this->sharedKey, payload);

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

void SoldierSendCoordsPage::sendTimerCallback(lv_timer_t *timer) {
    auto* self = static_cast<SoldierSendCoordsPage*>(timer->user_data);
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


std::pair<float, float> SoldierSendCoordsPage::getTileCenterLatLon(float lat, float lon, int zoomLevel, float tileSize) {

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

void SoldierSendCoordsPage::parseGPSData()
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
