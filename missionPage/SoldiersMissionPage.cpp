#include "SoldiersMissionPage.h"
#include <Commander.h>
#include <cstring>
#include <algorithm>

inline bool SoldiersMissionPage::isZero(float x)
{
    return std::fabs(x) < 1e-9f;
}

SoldiersMissionPage::SoldiersMissionPage(std::shared_ptr<LoraModule> loraModule,
     std::shared_ptr<GPSModule> gpsModule, std::unique_ptr<FHFModule> fhfModule, 
     std::unique_ptr<Soldier> soldierModule, bool commanderChange, bool fakeGPS)
{
    Serial.println("SoldiersMissionPage::SoldiersMissionPage");

    this->loraModule = std::move(loraModule);
    this->gpsModule = std::move(gpsModule);
    this->fhfModule = std::move(fhfModule);
    this->soldierModule = std::move(soldierModule);

    Serial.printf("🔍 Checking modules for %d:\n", this->soldierModule->getSoldierNumber());
    Serial.printf("📡 loraModule: %s\n", this->loraModule ? "✅ OK" : "❌ NULL");
    Serial.printf("📍 gpsModule: %s\n", this->gpsModule ? "✅ OK" : "❌ NULL");
    Serial.printf("📻 fhfModule: %s\n", this->fhfModule ? "✅ OK" : "❌ NULL");
    Serial.printf("📻 soldierModule: %s\n", this->soldierModule ? "✅ OK" : "❌ NULL");
    Serial.printf("📌 loraModule shared_ptr address: %p\n", this->loraModule.get());

    this->delaySendFakeGPS = false;
    Serial.printf("commanderChange %s\n", commanderChange ? "true" : "false");

    Serial.println("this->loraModule->setOnReadData");
    this->mainPage = lv_scr_act();

    Serial.printf("lv_scr_act: %p\n", (void*)this->mainPage);

    this->fakeGPS = fakeGPS;
    this->commanderSwitchEvent = commanderChange;
    this->currentIndex = 0;

    this->coordCount = 5;

    this->finishTimer = false;

    if(this->commanderSwitchEvent)
    {
        this->loraModule->setOnFileReceived([this](const uint8_t* data, size_t len) {
            this->receiveShamirRequest(data, len);
        });

        this->loraModule->setOnReadData([this](const uint8_t* data, size_t len) {
            this->loraModule->onLoraFileDataReceived(data, len);
        });
    }
    else
    {
        this->loraModule->setOnFileReceived([this](const uint8_t* data, size_t len) {
            this->onDataReceived(data, len);
        });

        this->loraModule->setOnReadData([this](const uint8_t* data, size_t len) {
            this->loraModule->onLoraFileDataReceived(data, len);
        });
    }

     // Serial.println("this->wifiModule->isConnected()"); // Removed because wifiModule is not a member of SoldiersMissionPage
}

void SoldiersMissionPage::createPage()
{
    Serial.println("SoldiersMissionPage::createPage");
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
        this->sendTimer = lv_timer_create(SoldiersMissionPage::sendTimerCallback, 7000, this);
    }
    else
    {

    }

    this->mainLoopTimer = lv_timer_create([](lv_timer_t* t){
        auto *self = static_cast<SoldiersMissionPage*>(t->user_data);
        self->loraModule->handleCompletedOperation();

        if(self->finishTimer)
        {
            return;
        }

        if (!self->loraModule->isBusy()) {
            self->loraModule->readData();
        }

        self->loraModule->syncFrequency(self->fhfModule.get());
    }, 100, this);
    
}

void SoldiersMissionPage::onDataReceived(const uint8_t* data, size_t len)
{

    size_t index = 0;

    if (len < 4) {
        Serial.println("Received data too short for any struct with length prefix");
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
        Serial.println("Decoded data too short for any struct with length prefix");
        return;
    }

    Serial.printf("PAYLOAD LEN: %d %u %u\n", base64Str.length(), 
     static_cast<uint8_t>(decodedData[0]), static_cast<uint8_t>(decodedData[1]));

    SwitchGMK sgPayload;
    sgPayload.msgID = static_cast<uint8_t>(decodedData[index++]);
    sgPayload.soldiersID = static_cast<uint8_t>(decodedData[index++]);

    if(sgPayload.soldiersID != this->soldierModule->getSoldierNumber())
    {
        Serial.println("Message wasn't for me!");
        return;
    }
    else if(sgPayload.msgID == 0x01)
    {
        sgPayload.encryptedGMK.assign(decodedData.begin() + 2, decodedData.end());
        this->onGMKSwitchEvent(sgPayload);

        Serial.printf("Deserialized SwitchGMK: msgID=%d, soldiersID=%d, encryptedGMK size=%zu\n",
                  sgPayload.msgID, sgPayload.soldiersID, sgPayload.encryptedGMK.size());
    }
    else if(sgPayload.msgID == 0x02)
    {
        this->loraModule->setOnReadData(nullptr);
        this->loraModule->setOnFileReceived(nullptr);
        this->finishTimer = true;


        SwitchCommander scPayload;
        scPayload.msgID = sgPayload.msgID;
        scPayload.soldiersID = sgPayload.soldiersID;

        uint16_t shamirLen = (decodedData[index] << 8) | decodedData[index + 1];
        scPayload.shamirPart.reserve(shamirLen);
        index += 2;

        if (decodedData.size() < index + shamirLen * 2 + 2) {
            Serial.println("❌ Not enough data for Shamir part");
            return;
        }

        for(int k = 0; k < shamirLen; ++k) {
            uint8_t x = uint8_t(decodedData[index++]);
            uint8_t y = uint8_t(decodedData[index++]);
            scPayload.shamirPart.emplace_back(x,y);
        }

        scPayload.shamirPartLength = shamirLen;
        
        uint8_t compromisedSoldiersLen = decodedData[index++];

        scPayload.compromisedSoldiers.assign(decodedData.begin() + index,
        decodedData.begin() + index + compromisedSoldiersLen);
        index += compromisedSoldiersLen;
        scPayload.compromisedSoldiersLength = compromisedSoldiersLen;

        uint8_t missingSoldiersLen = decodedData[index++];

        scPayload.missingSoldiers.assign(decodedData.begin() + index,
        decodedData.begin() + index + missingSoldiersLen);
        index += missingSoldiersLen;
        scPayload.missingSoldiersLength = missingSoldiersLen;

        Serial.printf("Important vars: %d %d %d\n", shamirLen, compromisedSoldiersLen, missingSoldiersLen);
        this->onCommanderSwitchEvent(scPayload);

        Serial.println("scPayload.compromisedSoldiers");
        for(const auto& commanderComp: scPayload.compromisedSoldiers)
        {
            Serial.println(commanderComp);
        }

        Serial.println("getCommandersInsertionOrder");
        for(const auto& commanderOrdered: this->soldierModule->getCommandersInsertionOrder())
        {
            Serial.println(commanderOrdered);
        }

        Serial.println("scPayload.missingSoldiers");
        for(const auto& missingSoldier: scPayload.missingSoldiers)
        {
            Serial.println(missingSoldier);
        }

        if(!this->soldierModule->getCommandersInsertionOrder().empty())
        {
            this->soldierModule->removeFirstCommanderFromInsertionOrder();
        }

        Serial.println("this->soldierModule->removeFirstCommanderFromInsertionOrder()");

        std::vector<uint8_t> commandersInsertionOrder = this->soldierModule->getCommandersInsertionOrder();

        if(!commandersInsertionOrder.empty() && commandersInsertionOrder.at(0) == this->soldierModule->getSoldierNumber())
        {
            this->onSoldierTurnToCommanderEvent(scPayload);
            
        }
        else
        {
            Serial.println("Im not supposed to be the next commander! waiting for shamirReq!");
            this->finishTimer = false;
            this->commanderSwitchEvent = true;

            this->loraModule->setOnFileReceived([this](const uint8_t* data, size_t len) {
                this->receiveShamirRequest(data, len);
            });
            
            this->loraModule->setOnReadData([this](const uint8_t* data, size_t len) {
                this->loraModule->onLoraFileDataReceived(data, len);
            });
        }
    }
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
    this->delaySendFakeGPS = true;

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

    if(self->commanderSwitchEvent)
    {
        return;
    }

    Serial.println("sendTimerCallback");
    Serial.println(self->currentIndex);

    if(self->currentIndex < 20) {

        for(int i = 0; i < 5; ++i)
        {
            if(self->loraModule->cancelReceive())
            {
                break;
            }
            delay(1);
        }

        if(!self->loraModule->cancelReceive())
        {
            return;
        }

        if (self->delaySendFakeGPS)
        {
            delay(10000);
            self->delaySendFakeGPS = false;
        }

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

        float currentLat, currentLon;
        uint8_t currentHeartRate = SoldiersMissionPage::generateHeartRate();
        uint8_t ID = self->soldierModule->getSoldierNumber();

        SoldiersMissionPage::generateNearbyCoordinates(self->coords[0].posLat, self->coords[0].posLon,
             20, currentLat, currentLon);

            

        self->sendCoordinate(currentLat, currentLon, currentHeartRate, ID);
        
        const char *current_text = lv_label_get_text(self->sendLabel);
        
        lv_label_set_text_fmt(self->sendLabel, "%s%s - sent coords {%.5f, %.5f}\n", 
                            current_text, timeStr, 
                            self->coords[self->currentIndex % self->coordCount].posLat, 
                            self->coords[self->currentIndex % self->coordCount].posLon);
        
        Serial.println("Finished sendTimerCallback");                    
        self->currentIndex++;
    } else {
        lv_timer_del(timer);
        self->currentIndex = 0;
    }
}

void SoldiersMissionPage::setTransferFunction(std::function<void(std::shared_ptr<LoraModule>, std::shared_ptr<GPSModule>,
     std::unique_ptr<FHFModule>, std::unique_ptr<Commander>)> cb)
{
    this->transferFunction = cb;
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

void SoldiersMissionPage::onCommanderSwitchEvent(SwitchCommander& payload)
{
    Serial.println("Received share");
    char sharePath[25];
    snprintf(sharePath, sizeof(sharePath), "/share_%u.txt", payload.soldiersID);
    Serial.println(sharePath);

    File currentShare = FFat.open(sharePath, FILE_WRITE);
    if (!currentShare)
    {
        Serial.print("Failed to open file to save share: ");
        Serial.println(sharePath);
        return;
    }

    for(auto &pt : payload.shamirPart) 
    {
        currentShare.printf("%u,%u\n", pt.first, pt.second);
    }

    currentShare.close();

    payload.shamirPart.clear();

    this->soldierModule->updateInsertionOrderByForbidden(payload.compromisedSoldiers);
    this->soldierModule->updateInsertionOrderByForbidden(payload.missingSoldiers);
}

void SoldiersMissionPage::onSoldierTurnToCommanderEvent(SwitchCommander& payload)
{
    Serial.println("onSoldierTurnToCommanderEvent");
    if(this->sendTimer)
    {
        this->currentIndex = 100;
        lv_timer_del(this->sendTimer);
    }

    std::unique_ptr<Commander> command = std::make_unique<Commander>(this->soldierModule->getName(),
    this->soldierModule->getPublicCert(), this->soldierModule->getPrivateKey(),
    this->soldierModule->getCAPublicCert(),
    this->soldierModule->getSoldierNumber(), this->soldierModule->getIntervalMS());

    Serial.println("command");
    command->setComp(payload.compromisedSoldiers);
    command->setSoldiers(this->soldierModule->getSoldiers());
    command->setCommanders(this->soldierModule->getCommanders());
    command->setInsertionOrders(this->soldierModule->getCommandersInsertionOrder());
    command->setMissing(payload.missingSoldiers);

    Serial.println("command->setInsertionOrders");
    payload.compromisedSoldiers.clear();
    payload.missingSoldiers.clear();
    this->soldierModule->clear();

    Serial.println("soldierModule->clear()");
    this->destroyPage();
    delay(10);

    lv_timer_del(this->mainLoopTimer);

    Serial.println("this->destroyPage()");

    Serial.printf("📦 Address of std::function cb variable: %p\n", (void*)&this->loraModule->getOnFileReceived());

    
    this->transferFunction(this->loraModule, this->gpsModule, std::move(this->fhfModule), std::move(command));
}

void SoldiersMissionPage::receiveShamirRequest(const uint8_t* data, size_t len)
{
    Serial.println("SoldiersMissionPage::receiveShamirRequest");
    
    if (len < 4)
    {
        Serial.println("Received data too short for any struct with length prefix");
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
        Serial.println("Decoded data too short for any struct with length prefix");
        return;
    }

    Serial.printf("PAYLOAD LEN: %d %u %u\n", base64Str.length(), 
     static_cast<uint8_t>(decodedData[0]), static_cast<uint8_t>(decodedData[1]));

    requestShamir payload;
    payload.msgID = static_cast<uint8_t>(decodedData[0]);
    payload.soldiersID = static_cast<uint8_t>(decodedData[1]);

    if(payload.soldiersID != this->soldierModule->getSoldierNumber() || payload.msgID != 0x04)
    {
        Serial.println("Message wasn't for me or message number is not matching current event!");
        return;
    }

    sendShamir ans;
    ans.msgID = 0x03;
    ans.soldiersID = payload.soldiersID;

    char sharePath[25];
    snprintf(sharePath, sizeof(sharePath), "/share_%u.txt", payload.soldiersID);

    File currentShamir = FFat.open(sharePath, FILE_READ);
    if (!currentShamir) 
    {
        Serial.print("❌ Failed to open share file: ");
        Serial.println(sharePath);

        return;
    }

    while (currentShamir.available()) 
    {
        String line = currentShamir.readStringUntil('\n');
        int comma = line.indexOf(',');
        if (comma < 1) continue;

        uint8_t x = (uint8_t)line.substring(0, comma).toInt();
        uint8_t y = (uint8_t)line.substring(comma + 1).toInt();
        ans.shamirPart.emplace_back(x, y);
    }

    currentShamir.close();

    FFatHelper::deleteFile(sharePath);

    std::string buffer;
    buffer.reserve(2 + ans.shamirPart.size() * 2);

    buffer += static_cast<char>(ans.msgID);
    buffer += static_cast<char>(ans.soldiersID);
    for(auto &pt : ans.shamirPart) 
    {
        buffer.push_back((char)pt.first);
        buffer.push_back((char)pt.second);
    }

    std::string base64Payload = crypto::CryptoModule::base64Encode(
    reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());
    
    Serial.printf("Important vars: %d %d\n", ans.msgID, ans.soldiersID);
    Serial.println(base64Payload.c_str());
    this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(base64Payload.c_str()), base64Payload.length());
    

    delay(10000);
    this->commanderSwitchEvent = false;
    this->currentIndex = 0;

    this->loraModule->setOnFileReceived([this](const uint8_t* data, size_t len) 
    {
        this->onDataReceived(data, len);
    });
}


void SoldiersMissionPage::generateNearbyCoordinates(float centerLat, float centerLon, float radiusMeters,
                               float& outLat, float& outLon) {

    float distance = static_cast<float>(rand()) / RAND_MAX * radiusMeters;
    float angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;

    float deltaLat = distance * std::cos(angle) / SoldiersMissionPage::ONE_DEG_LAT_IN_METERS;
    float deltaLon = distance * std::sin(angle) /
                     (SoldiersMissionPage::ONE_DEG_LAT_IN_METERS * std::cos(centerLat * M_PI / 180.0f));

    outLat = centerLat + deltaLat;
    outLon = centerLon + deltaLon;
}

uint8_t SoldiersMissionPage::generateHeartRate() {
    return rand() % 101 + 50;
}