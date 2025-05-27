#include <SoldierSendCoordsPage.h>

SoldiersSentData coords[] = {
  {0, 0, 31.970866, 34.785664,  78, 2},
  {0, 0, 31.970870, 34.785683, 100, 2},
  {0, 0, 31.970855, 34.785643,  55, 2}, 
  {0, 0, 31.970840, 34.785623,   0, 2},
  {0, 0, 31.970880, 34.785703, 120, 2}
};

SoldierSendCoordsPage::SoldierSendCoordsPage(std::unique_ptr<LoraModule> loraModule, std::unique_ptr<WifiModule> wifiModule, std::unique_ptr<GPSModule> gpsModule)
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

    lv_timer_t * sendTimer = lv_timer_create(SoldierSendCoordsPage::sendTimerCallback, 5000, NULL);
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
  
  loraModule->switchToReceiverMode();
  loraModule->setupListening();

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
    auto *self = static_cast<SoldierSendCoordsPage*>(timer->user_data);

    if(self->currentIndex < 20) {

        struct tm timeInfo;
        char timeStr[9];

        if(getLocalTime(&timeInfo)) {
            strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeInfo);
        } else {
            strcpy(timeStr, "00:00:00");
        }
        
        sendCoordinate(coords[self->currentIndex % self->coordCount].posLat, coords[self->currentIndex % self->coordCount].posLon,
        coords[self->currentIndex % self->coordCount].heartRate, coords[self->currentIndex % self->coordCount].soldiersID);
        
        const char *current_text = lv_label_get_text(self->sendLabel);
        
        lv_label_set_text_fmt(self->sendLabel, "%s%s - sent coords {%.5f, %.5f}\n", 
                            current_text, timeStr, 
                            coords[self->currentIndex % self->coordCount].posLat, 
                            coords[self->currentIndex % self->coordCount].posLon);
                            
        self->currentIndex++;
    } else {
        lv_timer_del(timer);
        self->currentIndex = 0;
    }
}
