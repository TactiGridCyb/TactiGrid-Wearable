#include "SoldiersMissionPage.h"
#include <Commander.h>
#include <cstring>
#include <algorithm>
#include "../certModule/certModule.h"

SoldiersMissionPage* SoldiersMissionPage::s_pmuOwner = nullptr;

void SoldiersMissionPage::pmuISR()
{
    if (s_pmuOwner) 
    {
        s_pmuOwner->pmuFlag = true;
    }
}

inline bool SoldiersMissionPage::isZero(float x)
{
    return std::fabs(x) < 1e-9f;
}

SoldiersMissionPage::SoldiersMissionPage(std::shared_ptr<LoraModule> loraModule,
     std::shared_ptr<GPSModule> gpsModule, std::unique_ptr<FHFModule> fhfModule, 
     std::unique_ptr<Soldier> soldierModule, bool commanderChange, bool fakeGPS)
{
    Serial.println("SoldiersMissionPage::SoldiersMissionPage");
    
    this->pmuFlag = false;
    s_pmuOwner = this;

    watch.attachPMU(&SoldiersMissionPage::pmuISR);

    watch.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    watch.clearPMU();
    watch.enableIRQ(
        XPOWERS_AXP2101_BAT_INSERT_IRQ   | XPOWERS_AXP2101_BAT_REMOVE_IRQ   |
        XPOWERS_AXP2101_VBUS_INSERT_IRQ  | XPOWERS_AXP2101_VBUS_REMOVE_IRQ  |
        XPOWERS_AXP2101_PKEY_SHORT_IRQ   | XPOWERS_AXP2101_PKEY_LONG_IRQ    |
        XPOWERS_AXP2101_BAT_CHG_DONE_IRQ | XPOWERS_AXP2101_BAT_CHG_START_IRQ
    );


    this->loraModule = std::move(loraModule);
    this->gpsModule = std::move(gpsModule);
    this->fhfModule = std::move(fhfModule);
    this->soldierModule = std::move(soldierModule);

    this->leadershipEvaluator = LeadershipEvaluator();

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
    Serial.printf("CURRENT GK: %s\n", crypto::CryptoModule::keyToHex(this->soldierModule->getGK()).c_str());
    this->fakeGPS = fakeGPS;
    this->commanderSwitchEvent = commanderChange;
    this->prevCommanderSwitchEvent = commanderSwitchEvent;
    this->initialCommanderSwitchEvent = commanderChange;

    this->currentIndex = 0;

    this->syncFreq = true;

    this->coordCount = 5;

    this->tileX = -1;
    this->tileY = -1;
    this->tileZoom = 0;

    this->finishTimer = false;

    String ownCert;

    FFatHelper::readFile(this->certPath, ownCert);

    SoldierInfo ownInfo;
    mbedtls_x509_crt_init(&ownInfo.cert);
    if (mbedtls_x509_crt_parse(&ownInfo.cert, reinterpret_cast<const unsigned char*>(ownCert.c_str()), ownCert.length() + 1) != 0)
    {
        mbedtls_x509_crt_free(&ownInfo.cert);
    }

    ownInfo.name = std::move(this->soldierModule->getName());
    ownInfo.soldierNumber = this->soldierModule->getSoldierNumber();
    ownInfo.status = SoldiersStatus::REGULAR;
    ownInfo.lastTimeReceivedData = millis();

    this->soldierModule->addSoldier(std::move(ownInfo));

    if(this->commanderSwitchEvent)
    {
        this->syncFreq = false;

        this->loraModule->setOnFileReceived([this](const uint8_t* data, size_t len) {
            this->onCommanderSwitchDataReceived(data, len, nullptr);
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

SoldiersMissionPage::~SoldiersMissionPage()
{
    Serial.printf("INSIDE THE DESTRUCTOR OF SoldiersMissionPage");

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
        this->sendTimer = lv_timer_create(SoldiersMissionPage::sendTimerCallback, 10000, this);
    }
    else
    {
        this->sendRealGPSTimer = lv_timer_create(SoldiersMissionPage::sendRealGPSTimerCallback, 10000, this);
    }

    this->mainLoopTimer = lv_timer_create([](lv_timer_t* t){
        auto *self = static_cast<SoldiersMissionPage*>(t->user_data);
        self->loraModule->handleCompletedOperation();

        if(self->finishTimer)
        {
            return;
        }

        if(self->syncFreq)
        {
            self->loraModule->syncFrequency(self->fhfModule.get());
        }


        if (!self->loraModule->isBusy()) {
            self->loraModule->readData();
        }

        if (self->pmuFlag) {
            self->pmuFlag = false;
            uint32_t status = watch.readPMU();
            if (watch.isPekeyShortPressIrq()) 
            {
                lv_async_call([](void* user_data) {
                    auto* me = static_cast<SoldiersMissionPage*>(user_data);
                    me->simulateZero = true;

                }, self);
            }
            watch.clearPMU();
        }

        if(!self->fakeGPS)
        {
            self->gpsModule->updateCoords();
        }
    }, 100, this);
    
}

void SoldiersMissionPage::onDataReceived(const uint8_t* data, size_t len)
{

    size_t index = 0;

    if (len == 0) {
        Serial.println("Received data too short for any struct with length prefix");
        return;
    }

    crypto::Ciphertext ct = crypto::CryptoModule::decodeText(data, len);
    crypto::ByteVec pt;

    try
    {
        pt = crypto::CryptoModule::decrypt(this->soldierModule->getGK(), ct);
    } catch (const std::exception& e)
    {
        Serial.printf("Decryption failed: %s\n", e.what());
        return;
        
    }

    JsonDocument dataReceivedJson;
    DeserializationError e = deserializeJson(dataReceivedJson, String((const char*)pt.data(), pt.size()));
    if (e) { Serial.println("coords JSON parse failed"); return; }

    if(dataReceivedJson["msgID"] == 0x08)
    {
        if(this->commanderSwitchEvent == this->prevCommanderSwitchEvent)
        {
            Serial.println("RECEIVED SILENCE!");
            this->prevCommanderSwitchEvent = this->commanderSwitchEvent;
            this->commanderSwitchEvent = true;
            this->syncFreq = false;
        }

        return;
        
    }

    String tmp;
    serializeJson(dataReceivedJson, tmp);
    Serial.printf("%s %d\n", tmp.c_str(), dataReceivedJson.size());

    if (dataReceivedJson.size() < 2) 
    {
        Serial.println("Decoded data too short for any struct with length prefix");
        return;
    }
    
    Serial.printf("PAYLOAD LEN: %d %u\n", dataReceivedJson.size(), dataReceivedJson["msgID"].as<uint8_t>());
    
    if(dataReceivedJson["msgID"].as<uint8_t>() != 0x05 && dataReceivedJson["soldiersID"].as<uint8_t>() != this->soldierModule->getSoldierNumber())
    {
        Serial.println("Message wasn't for me!");
        return;
    }
    else if(dataReceivedJson["msgID"].as<uint8_t>() == 0x01)
    {
        this->simulateZero = false;
        this->onGMKSwitchEvent(dataReceivedJson);

        Serial.printf("Deserialized SwitchGMK: msgID=%d, soldiersID=%d, salt size=%zu\n",
                  dataReceivedJson["msgID"], dataReceivedJson["soldiersID"],
                   dataReceivedJson["salt"].to<JsonArray>().size());

        this->commanderSwitchEvent = this->prevCommanderSwitchEvent;
        this->syncFreq = true;
    }
    else if(dataReceivedJson["msgID"].as<uint8_t>() == 0x02)
    {
        this->loraModule->setOnReadData(nullptr);
        this->loraModule->setOnFileReceived(nullptr);
        this->finishTimer = true;

        JsonArray shamirPart = dataReceivedJson["shamirPart"].as<JsonArray>();
        JsonArray missingSoldiers = dataReceivedJson["missingSoldiers"].as<JsonArray>();
        JsonArray compromisedSoldiers = dataReceivedJson["compromisedSoldiers"].as<JsonArray>();
        JsonArray soldiersCoordsIDS = dataReceivedJson["soldiersCoordsIDS"].as<JsonArray>();
        JsonArray soldiersCoords = dataReceivedJson["soldiersCoords"].as<JsonArray>();


        Serial.printf("Important vars: %d %d\n", dataReceivedJson["compromisedSoldiers"], dataReceivedJson["missingSoldiers"]);
        this->onCommanderSwitchEvent(dataReceivedJson);

        Serial.println("scPayload.compromisedSoldiers");
        for(const auto& commanderComp : compromisedSoldiers)
        {
            Serial.println(commanderComp.as<uint8_t>());
        }

        Serial.println("getCommandersInsertionOrder");
        for(const auto& commanderOrdered: this->soldierModule->getCommandersInsertionOrder())
        {
            Serial.println(commanderOrdered);
        }

        Serial.println("scPayload.missingSoldiers");
        for(const auto& missingSoldier : missingSoldiers)
        {
            Serial.println(missingSoldier.as<uint8_t>());
        }

        Serial.println("scPayload.soldiersCoordsIDS");
        for(const auto& soldiersIDS : soldiersCoordsIDS)
        {
            Serial.println(soldiersIDS.as<uint8_t>());
        }

        Serial.println("scPayload.soldiersCoords");
        for(const auto& soldiersCoord: soldiersCoords)
        {
            auto arr = soldiersCoord.as<JsonArrayConst>();
            float lat = arr[0];
            float lon = arr[1];
            Serial.printf("%.5f %.5f\n", lat, lon);
        }

        for(uint8_t idx = 0; idx < soldiersCoordsIDS.size(); ++idx)
        {
            JsonArray coord = soldiersCoords[idx];
            Serial.printf("ID and coord soldier -> %d { %.3f %.3f }\n", soldiersCoordsIDS[idx].as<uint8_t>(), coord[0].as<float>(), coord[1].as<float>());
        }

        if(!this->soldierModule->getCommandersInsertionOrder().empty())
        {
            this->soldierModule->removeFirstCommanderFromInsertionOrder();
        }

        Serial.println("this->soldierModule->removeFirstCommanderFromInsertionOrder()");

        std::vector<uint8_t> commandersInsertionOrder = this->soldierModule->getCommandersInsertionOrder();
        bool canBeCommander = true;

        if(!commandersInsertionOrder.empty() && commandersInsertionOrder.at(0) == this->soldierModule->getSoldierNumber())
        {
            canBeCommander = this->canBeCommander(&dataReceivedJson, false);

            if(canBeCommander)
            {
                return;
            }

            this->soldierModule->removeFirstCommanderFromInsertionOrder();

            canBeCommander = false;
        }
        
        Serial.println("getCommandersInsertionOrder");
        for(const auto& commanderOrdered: this->soldierModule->getCommandersInsertionOrder())
        {
            Serial.println(commanderOrdered);
        }


        Serial.println("Im not supposed to be the next commander! waiting for shamirReq!");
            
        this->finishTimer = false;
        this->commanderSwitchEvent = true;

        if(!canBeCommander)
        {
            this->loraModule->setOnFileReceived([this](const uint8_t* data, size_t len) {
                this->onCommanderSwitchDataReceived(data, len, nullptr);
            });
        }
        else
        {
            this->loraModule->setOnFileReceived([this, &dataReceivedJson](const uint8_t* data, size_t len) mutable {
                this->onCommanderSwitchDataReceived(data, len, &dataReceivedJson);
            });
        }

        this->loraModule->setOnReadData([this](const uint8_t* data, size_t len) {
            this->loraModule->onLoraFileDataReceived(data, len);
        });
    }
}

void SoldiersMissionPage::onGMKSwitchEvent(JsonDocument payload)
{
    Serial.printf("Decryption failed in onGMKSwitchEvent111222 %d\n",this->soldierModule->getCommandersInsertionOrder().size());
    
    std::string decryptedSalt;
    std::vector<uint8_t> vectorSalt;
    JsonArray arr = payload["salt"].as<JsonArray>();
    vectorSalt.reserve(arr.size());
    for (JsonVariant x : arr) vectorSalt.push_back(x.as<uint8_t>());

    bool success = certModule::decryptWithPrivateKey(this->soldierModule->getPrivateKey(), vectorSalt, decryptedSalt);

    if (!success)
    {
        Serial.println("Decryption failed in onGMKSwitchEvent");
        return;
    }

    Serial.println("ENCRYPTED SALT: ");
    Serial.printf("NEW SALT SET: %s\n", decryptedSalt.c_str());
    
    for(const auto& v : this->soldierModule->getCommandersInsertionOrder())
    {
        Serial.printf("%d\n", v);
    }

    crypto::ByteVec saltVec(reinterpret_cast<const uint8_t*>(decryptedSalt.data()), reinterpret_cast<const uint8_t*>(decryptedSalt.data()) + decryptedSalt.size());
    const std::string infoStr = payload["info"].as<const char*>();
    const crypto::Key256 newGK = crypto::CryptoModule::deriveGK(this->soldierModule->getGMK(), payload["millis"].as<uint64_t>(), infoStr, saltVec, this->soldierModule->getCommandersInsertionOrder().at(0));
    
    //TODO - payload["info"]
    Serial.printf("NEW GMK: %s\n", crypto::CryptoModule::keyToHex(newGK).c_str());

    this->soldierModule->setGK(newGK);
    this->delaySendFakeGPS = true;

}

void SoldiersMissionPage::sendCoordinate(float lat, float lon, uint16_t heartRate, uint16_t soldiersID) {
    Serial.println("sendCoordinate");
    
    Serial.printf("----------------------\nCurrent percentage: %d%\n-----------------------------", watch.getBatteryPercent());


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
    
    crypto::Ciphertext ct = crypto::CryptoModule::encrypt(this->soldierModule->getGK(), payload);

    String msg = crypto::CryptoModule::encodeCipherText(ct);

    int16_t transmissionState = -1;

    for(int i = 0; i < 5; ++i)
    {
        delay(random(0, 100));
        if(loraModule->canTransmit())
        {
            transmissionState = loraModule->sendData(msg.c_str());
            break;
        }
    }

    if(transmissionState == -1)
    {
        return;
    }

    struct tm timeInfo;
    char timeStr[9];

    if(getLocalTime(&timeInfo)) {
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeInfo);
    } else {
        strcpy(timeStr, "00:00:00");
    }

    lv_label_set_text_fmt(sendLabel, "%s - sent coords {%.5f, %.5f}\n with freq %.2f\n", 
                            timeStr,
                            lat, 
                            lon,
                            this->loraModule->getCurrentFreq());
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

        if(self->tileZoom == 0)
        {
            std::pair<float, float> tileLatLon = getTileCenterLatLon(self->coords[0].posLat,
                 self->coords[0].posLon, 19, 256);

            std::tuple<int,int,int> tileLocation = SoldiersMissionPage::positionToTile(std::get<0>(tileLatLon),
             std::get<1>(tileLatLon), 19);
            
            self->tileZoom = std::get<0>(tileLocation);
            self->tileX = std::get<1>(tileLocation);
            self->tileY = std::get<2>(tileLocation);
            
            Serial.printf("First time setting coords! %d %d %d\n", self->tileX, self->tileY, self->tileZoom);
            
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
        uint8_t currentHeartRate;
        uint8_t ID = self->soldierModule->getSoldierNumber();
        currentHeartRate = self->generateHeartRate();

        LVGLPage::generateNearbyCoordinatesFromTile(self->tileX, self->tileY,
             self->tileZoom, currentLat, currentLon);

        self->sendCoordinate(currentLat, currentLon, currentHeartRate, ID);

        self->soldierModule->updateLocation(self->soldierModule->getSoldierNumber(), currentLat, currentLon, true);
        
        const char *current_text = lv_label_get_text(self->sendLabel);
        
        lv_label_set_text_fmt(self->sendLabel, "%s%s - sent coords {%.5f, %.5f}\n", 
                            current_text, timeStr, 
                            currentLat, 
                            currentLon);
        
        Serial.println("Finished sendTimerCallback");                    
        self->currentIndex++;
    } else {
        self->finishTimer = true;
        lv_timer_del(self->mainLoopTimer);
        lv_timer_del(timer);
    }
}

void SoldiersMissionPage::sendRealGPSTimerCallback(lv_timer_t *timer)
{
    auto* self = static_cast<SoldiersMissionPage*>(timer->user_data);

    if(self->commanderSwitchEvent)
    {
        return;
    }

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

    Serial.println("sendRealGPSTimerCallback");

    self->parseGPSData();
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
        sendCoordinate(lat, lon, LVGLPage::generateHeartRate(), this->soldierModule->getSoldierNumber());

        this->soldierModule->updateLocation(this->soldierModule->getSoldierNumber(), lat, lon, true);
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
        lv_label_set_text_fmt(this->sendLabel, "Sats:%u\nHDOP:%.1f\nLat:%.5f\nLon:%.5f\nDate:%d/%d/%d \nTime:%d/%d/%d\nAlt:%.2f m \nSpeed:%.2f\n Freq:%.2f",
                            satellites, hdop, lat, lon, year, month, day, hour, minute, second, meters, kmph, this->loraModule->getCurrentFreq());
    }
}

void SoldiersMissionPage::onCommanderSwitchEvent(JsonDocument& payload)
{
    Serial.println("Received share");
    char sharePath[25];
    snprintf(sharePath, sizeof(sharePath), "/share_%u.txt", payload["soldiersID"].as<uint8_t>());
    Serial.println(sharePath);

    File currentShare = FFat.open(sharePath, FILE_WRITE);
    if (!currentShare)
    {
        Serial.print("Failed to open file to save share: ");
        Serial.println(sharePath);
        return;
    }

    JsonArray arr = payload["shamirPart"].as<JsonArray>();
    for (const auto& row : arr) 
    {
        currentShare.printf("%u,%u\n", payload["soldiersID"].as<uint8_t>(), row.as<uint16_t>());
    }

    currentShare.close();


    this->soldierModule->updateInsertionOrderByForbidden(payload["compromisedSoldiers"].as<JsonArray>());
    this->soldierModule->updateInsertionOrderByForbidden(payload["missingSoldiers"].as<JsonArray>());
}

void SoldiersMissionPage::onSoldierTurnToCommanderEvent(JsonDocument& payload, bool skipEvent)
{

    Serial.println("onSoldierTurnToCommanderEvent");
    if(this->fakeGPS && this->sendTimer)
    {
        this->currentIndex = 100;
        lv_timer_del(this->sendTimer);
    }
    else if(!this->fakeGPS && this->sendRealGPSTimer)
    {
        lv_timer_del(this->sendRealGPSTimer);
    }

    Serial.println("post timer deletions");
    if(this->soldierModule->getName().data())
    {
        Serial.println("Has Name!");
    }

    if(this->soldierModule->getPublicCert().raw.len > 0)
    {
        Serial.println("Has getPublicCert!");
    }

    if(this->soldierModule->getPrivateKey().pk_info != NULL)
    {
        Serial.println("Has getPrivateKey!");
    }

    if(this->soldierModule->getCAPublicCert().raw.len > 0)
    {
        Serial.println("Has getCAPublicCert!");
    }

    if(this->soldierModule->getSoldierNumber() > 0)
    {
        Serial.printf("Has getSoldierNumber! %d\n", this->soldierModule->getSoldierNumber());
    }

    if(this->soldierModule->getIntervalMS() > 0)
    {
        Serial.printf("Has getIntervalMS! %d\n", this->soldierModule->getIntervalMS());
    }

    std::unique_ptr<Commander> command = std::make_unique<Commander>(this->soldierModule->getName(),
    this->soldierModule->getPublicCert(), this->soldierModule->getPrivateKey(),
    this->soldierModule->getCAPublicCert(),
    this->soldierModule->getSoldierNumber(), this->soldierModule->getIntervalMS());

    command->setGMK(this->soldierModule->getGMK());
    command->setGK(this->soldierModule->getGK());
    command->setDirectCommander(!skipEvent);
    Serial.println("command");

    this->soldierModule->removeSoldier(this->soldierModule->getSoldierNumber());
    JsonArray compromisedSoldiersArr = payload["compromisedSoldiers"].as<JsonArray>();
    JsonArray missingSoldiersArr = payload["missingSoldiers"].as<JsonArray>();

    std::vector<uint8_t> compromisedSoldiers;
    compromisedSoldiers.reserve(compromisedSoldiersArr.size());

    for (JsonVariant v : compromisedSoldiersArr) 
    {
        int x = v.as<int>();
        compromisedSoldiers.push_back(static_cast<uint8_t>(x));
    }

    command->setComp(compromisedSoldiers);

    compromisedSoldiers.clear();
    compromisedSoldiers.reserve(missingSoldiersArr.size());
    for (JsonVariant v : missingSoldiersArr) 
    {
        int x = v.as<int>();
        compromisedSoldiers.push_back(static_cast<uint8_t>(x));
    }

    command->setSoldiers(this->soldierModule->getSoldiers());
    command->setCommanders(this->soldierModule->getCommanders());
    command->setInsertionOrders(this->soldierModule->getCommandersInsertionOrder());
    command->setMissing(compromisedSoldiers);

    Serial.println("command->setInsertionOrders");

    Serial.println("soldierModule->clear()");
    this->destroyPage();
    delay(10);

    lv_timer_del(this->mainLoopTimer);

    Serial.println("this->destroyPage()");

    Serial.printf("📦 Address of std::function cb variable: %p\n", (void*)&this->loraModule->getOnFileReceived());

    if (s_pmuOwner == this)
    {
         s_pmuOwner = nullptr;
    }
    
    watch.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    watch.clearPMU();


    this->transferFunction(this->loraModule, this->gpsModule, std::move(this->fhfModule), std::move(command));
}

void SoldiersMissionPage::transmitSkipCommanderMessage()
{
    Serial.println("transmitSkipCommanderMessage");

    JsonDocument skipCommanderDoc;
    skipCommanderDoc["msgID"] = 0x05;
    skipCommanderDoc["soldiersID"] = 0;
    skipCommanderDoc["commandersID"] = this->soldierModule->getSoldierNumber();;

    String skipCommanderJson;
    serializeJson(skipCommanderDoc, skipCommanderJson);

    crypto::ByteVec payload(skipCommanderJson.begin(), skipCommanderJson.end());
    crypto::Ciphertext ct = crypto::CryptoModule::encrypt(this->soldierModule->getGK(), payload);

    String msg = crypto::CryptoModule::encodeCipherText(ct);

    delay(1000);

    this->loraModule->cancelReceive();
    this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());
}

void SoldiersMissionPage::onCommanderSwitchDataReceived(const uint8_t* data, size_t len, JsonDocument* scPayload)
{
    Serial.println("onCommanderSwitchDataReceived");

    crypto::Ciphertext ct = crypto::CryptoModule::decodeText(data, len);
    crypto::ByteVec pt;

    try 
    {
        pt = crypto::CryptoModule::decrypt(this->soldierModule->getGK(), ct);
    } catch (const std::exception& e)
    {
        Serial.printf("Decryption failed: %s\n", e.what());
        return;
    }

    JsonDocument commmanderSwitchDoc;
    DeserializationError e = deserializeJson(commmanderSwitchDoc, String((const char*)pt.data(), pt.size()));
    if (e) { Serial.println("coords JSON parse failed"); return; }

    JsonDocument skipCommanderDoc;

    uint8_t msgID = commmanderSwitchDoc["msgID"].as<uint8_t>();
    
    skipCommanderDoc["msgID"] = msgID;

    if(msgID == 0x06)
    {
        Serial.println("Commander Found!!!");

        delay(3000);

        this->syncFreq = true;
        this->commanderSwitchEvent = false;
        this->prevCommanderSwitchEvent = this->commanderSwitchEvent;

        this->currentIndex = 0;

        this->loraModule->setOnFileReceived([this](const uint8_t* data, size_t len) 
        {
            this->onDataReceived(data, len);
        });

        return;
    }
    else if(msgID == 0x07)
    {
        if(!scPayload && !this->initialCommanderSwitchEvent)
        {
            Serial.println("!SCPAYLOAD!");
            delay(500);
            this->transmitSkipCommanderMessage();
        }
        return;
    }

    if(msgID == 0x05)
    {
        uint8_t commandersID = commmanderSwitchDoc["commandersID"].as<uint8_t>();
        Serial.printf("Received SkipCommander! %d %d\n", msgID, commandersID);
        if(this->soldierModule->getCommandersInsertionOrder().at(0) != commandersID)
        {
            Serial.println("Problematic! Skipped commander is somehow not first in line!");
            return;
        }

        this->soldierModule->removeFirstCommanderFromInsertionOrder();

        Serial.println("getCommandersInsertionOrder");
        for(const auto& commanderOrdered: this->soldierModule->getCommandersInsertionOrder())
        {
            Serial.println(commanderOrdered);
        }

        bool canBeCommander = true;

        if(!this->soldierModule->getCommandersInsertionOrder().empty() && this->soldierModule->getCommandersInsertionOrder().at(0) == this->soldierModule->getSoldierNumber())
        {
            Serial.println("I should be commander now after the previous skipped!");

            canBeCommander = this->canBeCommander(scPayload, true);

            if(canBeCommander)
            {
                return;
            }

            this->soldierModule->removeFirstCommanderFromInsertionOrder();

            Serial.println("getCommandersInsertionOrder11");

            for(const auto& commanderOrdered: this->soldierModule->getCommandersInsertionOrder())
            {
                Serial.println(commanderOrdered);
            }

            canBeCommander = false;

            this->loraModule->setOnFileReceived([this](const uint8_t* data, size_t len) {
                this->onCommanderSwitchDataReceived(data, len, nullptr);
            });

            delay(500);

            this->transmitSkipCommanderMessage();
        }

    }
    else if(msgID == 0x04 && commmanderSwitchDoc["soldiersID"].as<uint8_t>() == this->soldierModule->getSoldierNumber())
    {
        Serial.println("After someone skipped or not skipped commandership, a new commander was found!");

        this->receiveShamirRequest(data, len);
    }
    else
    {
        Serial.printf("Something strange! these are the parameters: %d\n", msgID);
    }
}

bool SoldiersMissionPage::canBeCommander(JsonDocument* payload, bool skipEvent)
{
    Serial.println("CanBeCommander");
    if(!this->soldierModule->areCoordsValid(this->soldierModule->getSoldierNumber(), true))
    {
        return false;
        //Cannot be commander -> pass
    }

    std::pair<float,float> selfCoord;
    JsonArray soldiersCoordsIDS = (*payload)["soldiersCoordsIDS"].as<JsonArray>();
    JsonArray soldiersCoords = (*payload)["soldiersCoords"].as<JsonArray>();

    for (uint8_t i = 0; i < soldiersCoordsIDS.size(); ++i) 
    {
        if (soldiersCoordsIDS[i].as<uint8_t>() == this->soldierModule->getSoldierNumber()) 
        {
            float x = soldiersCoords[i][0].as<float>();
            float y = soldiersCoords[i][1].as<float>();
            selfCoord = {x, y};
        }
    }

    std::vector<std::pair<float, float>> coordsVector;
    coordsVector.reserve(soldiersCoords.size());
    for (JsonArray row : soldiersCoords) 
    {
        if (row.size() >= 2) {
            float x = row[0].as<float>();
            float y = row[1].as<float>();
            coordsVector.emplace_back(x, y);
        }
    }


    Serial.printf("%.5f %.5f\n", selfCoord.first, selfCoord.second);
    float score = this->leadershipEvaluator.calculateScore(coordsVector, selfCoord);
    Serial.printf("Score is: %.5f\n", score);
    if(score < 0.5)
    {
        return false;
        // Cannot be commander -> pass
    }

    this->onSoldierTurnToCommanderEvent(*payload, skipEvent);

    return true;


}

void SoldiersMissionPage::receiveShamirRequest(const uint8_t* data, size_t len)
{
    Serial.println("SoldiersMissionPage::receiveShamirRequest");
    
    crypto::Ciphertext ct = crypto::CryptoModule::decodeText(data, len);
    crypto::ByteVec pt;
    
    try 
    {
        pt = crypto::CryptoModule::decrypt(this->soldierModule->getGK(), ct);
    } catch (const std::exception& e)
    {
        Serial.printf("Decryption failed: %s\n", e.what());
        return;
        
    }

    JsonDocument requestReceivedJson;
    DeserializationError e = deserializeJson(requestReceivedJson, String((const char*)pt.data(), pt.size()));
    if (e) { Serial.println("coords JSON parse failed"); return; }

    String tmp;
    serializeJson(requestReceivedJson, tmp);
    Serial.printf("%s %d\n", tmp.c_str(), requestReceivedJson.size());

    if (requestReceivedJson.size() < 2)
    {
        Serial.println("Decoded data too short for any struct with length prefix");
        return;
    }

    

    if(requestReceivedJson["soldiersID"].as<uint8_t>() != this->soldierModule->getSoldierNumber() || requestReceivedJson["msgID"].as<uint8_t>() != 0x04)
    {
        Serial.println("Message wasn't for me or message number is not matching current event!");
        return;
    }


    JsonDocument shamirAnsDoc;
    shamirAnsDoc["msgID"] = 0x03;
    shamirAnsDoc["soldiersID"] = requestReceivedJson["soldiersID"].as<uint8_t>();

    char sharePath[25];
    snprintf(sharePath, sizeof(sharePath), "/share_%u.txt", requestReceivedJson["soldiersID"].as<uint8_t>());

    File currentShamir = FFat.open(sharePath, FILE_READ);
    if (!currentShamir) 
    {
        Serial.print("❌ Failed to open share file: ");
        Serial.println(sharePath);

        return;
    }

    JsonArray parts = shamirAnsDoc.createNestedArray("shamirPart");

    while (currentShamir.available()) 
    {
        String line = currentShamir.readStringUntil('\n');
        int comma = line.indexOf(',');
        if (comma < 1) continue;

        uint16_t x = (uint16_t)line.substring(0, comma).toInt();
        uint16_t y = (uint16_t)line.substring(comma + 1).toInt();

        parts.add(y);
    }

    currentShamir.close();
    FFatHelper::deleteFile(sharePath);
    
    String shamirAnsJson;
    serializeJson(shamirAnsDoc, shamirAnsJson);
    
    Serial.printf("Important vars: %d %d\n", requestReceivedJson["msgID"].as<uint8_t>(), requestReceivedJson["soldiersID"].as<uint8_t>());
    
    crypto::ByteVec payload(shamirAnsJson.begin(), shamirAnsJson.end());
    ct = crypto::CryptoModule::encrypt(this->soldierModule->getGK(), payload);
    String msg = crypto::CryptoModule::encodeCipherText(ct);
    
    this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());
}


std::tuple<int, int, int> SoldiersMissionPage::positionToTile(float lat, float lon, int zoom)
{
    float lat_rad = radians(lat);
    float n = pow(2.0, zoom);
    
    int x_tile = floor((lon + 180.0) / 360.0 * n);
    int y_tile = floor((1.0 - log(tan(lat_rad) + 1.0 / cos(lat_rad)) / PI) / 2.0 * n);
    
    return std::make_tuple(zoom, x_tile, y_tile);
}