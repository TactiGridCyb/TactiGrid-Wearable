#include "CommandersMissionPage.h"
#include "../certModule/certModule.h"

CommandersMissionPage* CommandersMissionPage::c_pmuOwner = nullptr;

void CommandersMissionPage::pmuISR()
{
    if (c_pmuOwner) 
    {
        c_pmuOwner->pmuFlag = true;
    }
}

CommandersMissionPage::CommandersMissionPage(std::shared_ptr<LoraModule> loraModule, std::unique_ptr<WifiModule> wifiModule,
     std::shared_ptr<GPSModule> gpsModule, std::unique_ptr<FHFModule> fhfModule,
    std::unique_ptr<Commander> commanderModule, const std::string& logFilePath, bool commanderChange, bool fakeGPS)
    : logFilePath(logFilePath) 
    {

        this->pmuFlag = false;
        c_pmuOwner = this;

        watch.attachPMU(&CommandersMissionPage::pmuISR);

        watch.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
        watch.clearPMU();
        watch.enableIRQ(
            XPOWERS_AXP2101_BAT_INSERT_IRQ   | XPOWERS_AXP2101_BAT_REMOVE_IRQ   |
            XPOWERS_AXP2101_VBUS_INSERT_IRQ  | XPOWERS_AXP2101_VBUS_REMOVE_IRQ  |
            XPOWERS_AXP2101_PKEY_SHORT_IRQ   | XPOWERS_AXP2101_PKEY_LONG_IRQ    |
            XPOWERS_AXP2101_BAT_CHG_DONE_IRQ | XPOWERS_AXP2101_BAT_CHG_START_IRQ
        );

        this->loraModule = std::move(loraModule);
        this->wifiModule = std::move(wifiModule);
        this->gpsModule = std::move(gpsModule);
        this->fhfModule = std::move(fhfModule);
        this->commanderModule = std::move(commanderModule);

        this->commanderSwitchEvent = commanderChange;
        this->initialCoordsReceived = false;

        this->tileImg = nullptr;

        this->fakeGPS = fakeGPS;
        this->finishMainTimer = false;

        this->initialShamirReceived = false;
        
        Serial.printf("🔍 Checking modules for %d:\n", this->commanderModule->getCommanderNumber());
        Serial.printf("📡 loraModule: %s\n", this->loraModule ? "✅ OK" : "❌ NULL");
        Serial.printf("🌐 wifiModule: %s\n", this->wifiModule ? "✅ OK" : "❌ NULL");
        Serial.printf("📍 gpsModule: %s\n", this->gpsModule ? "✅ OK" : "❌ NULL");
        Serial.printf("📻 fhfModule: %s\n", this->fhfModule ? "✅ OK" : "❌ NULL");
        Serial.printf("📻 commanderModule: %s\n", this->commanderModule ? "✅ OK" : "❌ NULL");
        Serial.printf("📻 isCommanderDirect: %s\n", this->commanderModule->isDirectCommander() ? "✅ OK" : "❌ NULL");

        Serial.printf("📌 loraModule shared_ptr address: %p\n", this->loraModule.get());
        Serial.printf("CURRENT GK: %s\n", crypto::CryptoModule::keyToHex(this->commanderModule->getGK()).c_str());
        this->mainPage = lv_scr_act();
        this->infoBox = LVGLPage::createInfoBox();
        
        this->tileZoom = 0;
        this->tileX = -1;
        this->tileY = -1;

        FFatHelper::deleteFile(this->tileFilePath);

        Serial.printf("this->commanderSwitchEvent: %s\n", (this->commanderSwitchEvent ? "true"  : "false"));

        if(!this->commanderSwitchEvent)
        {
            this->loraModule->setOnReadData([this](const uint8_t* data, size_t len) {
                this->onDataReceived(data, len);
            });

            this->loraModule->setOnFileReceived(nullptr);
        }
        else
        {
            FFatHelper::deleteFile(logFilePath.c_str());
            this->shamirPartsCollected = 0;
            this->currentShamirRec = this->commanderModule->getCommanderNumber();
            LVGLPage::restartInfoBoxFadeout(this->infoBox, 1000, 5000, "Switch Commander Event!");

            Serial.printf("Reserved %d places\n", this->commanderModule->getSoldiers().size() +
            this->commanderModule->getCommanders().size());
            this->didntSendShamir.reserve(this->commanderModule->getSoldiers().size() + this->commanderModule->getCommanders().size() - 
            this->commanderModule->getComp().size());
            this->didntSendShamir.push_back(this->commanderModule->getCommanderNumber());

            std::vector<uint8_t> soldiersKeys;

            soldiersKeys.reserve(this->commanderModule->getSoldiers().size());
            Serial.println("Soldiers:");
            for (auto const& kv : this->commanderModule->getSoldiers()) 
            {
                if(!this->commanderModule->isComp(kv.first))
                {
                    Serial.println(kv.first);
                    soldiersKeys.push_back(kv.first);
                }
                
            }

            this->didntSendShamir.insert(this->didntSendShamir.end(),
            soldiersKeys.begin(), soldiersKeys.end());

            soldiersKeys.clear();

            soldiersKeys.reserve(this->commanderModule->getCommanders().size());
            Serial.println("Commanders:");
            for (auto const& kv : this->commanderModule->getCommanders()) 
            {
                if(!this->commanderModule->isComp(kv.first) && kv.first != this->commanderModule->getCommanderNumber())
                {
                    Serial.println(kv.first);
                    soldiersKeys.push_back(kv.first);
                }
            }
            
            this->didntSendShamir.insert(this->didntSendShamir.end(),
            soldiersKeys.begin(), soldiersKeys.end());

            soldiersKeys.clear();

            Serial.println("this->didntSendShamir:");

            for (auto const& k : this->didntSendShamir) 
            {
                Serial.println(k);
            }


            if(this->commanderModule->isDirectCommander())
            {
                
                this->loraModule->setOnFileReceived([this](const uint8_t* data, size_t len) {
                    this->onFinishedSplittingShamirReceived(data, len);
                });

                this->loraModule->setOnReadData([this](const uint8_t* data, size_t len) {
                    this->loraModule->onLoraFileDataReceived(data, len);
                });
            }
            else
            {

                lv_async_call([](void* user_data) {
                    auto* me = static_cast<CommandersMissionPage*>(user_data);
                    
                    JsonDocument finishSplittingShamirDoc;

                    finishSplittingShamirDoc["msgID"] = 0x07;

                    String finishSplittingShamirJson;
                    serializeJson(finishSplittingShamirDoc, finishSplittingShamirJson);

                    crypto::ByteVec payload(finishSplittingShamirJson.begin(), finishSplittingShamirJson.end());

                    crypto::Ciphertext ct = crypto::CryptoModule::encrypt(me->commanderModule->getGK(), payload);
                    String msg = crypto::CryptoModule::encodeCipherText(ct);
                    
                    me->onFinishedSplittingShamirReceived(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());

                }, this);
                
            }
            

            
        }
    
        Serial.printf("📦 Address of std::function cb variable: %p\n", (void*)&this->loraModule->getOnFileReceived());

    }

void CommandersMissionPage::createPage() {
    Serial.println("CommandersMissionPage::createPage");
    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "TactiGrid Commander");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* waitingLabel = lv_label_create(mainPage);
    lv_obj_center(waitingLabel);
    lv_label_set_text_fmt(waitingLabel, "Waiting For Initial Coords with freq %.2f", this->loraModule->getCurrentFreq());
    lv_obj_set_style_text_color(waitingLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    Serial.println("this->regularLoopTimer");
    this->regularLoopTimer = lv_timer_create([](lv_timer_t* t){

        lv_async_call([](void* user_data) 
        {
            lv_timer_t* timer = static_cast<lv_timer_t*>(user_data);

            auto *me = static_cast<CommandersMissionPage*>(timer->user_data);

            me->loraModule->handleCompletedOperation();
            if(!me->commanderSwitchEvent)
            {
                me->loraModule->syncFrequency(me->fhfModule.get());
            }

            if(me->finishMainTimer)
            {
                me->loraModule->cancelReceive();
                lv_timer_del(timer);
                Serial.println("Removed Main Timer!");
                me->regularLoopTimer = nullptr;
                return;
            }
            
            if (!me->loraModule->isBusy()) 
            {
                me->loraModule->readData();
            }


            if (me->pmuFlag) {
                me->pmuFlag = false;
                uint32_t status = watch.readPMU();
                if (watch.isPekeyShortPressIrq()) 
                {
                    lv_async_call([](void* user_data) {
                        auto* me = static_cast<CommandersMissionPage*>(user_data);
                        me->switchCommanderEvent();

                    }, me);
                }
                watch.clearPMU();
            }

            me->gpsModule->updateCoords();

        }, t);
    }, 100, this);

    this->selfLogTimer = lv_timer_create([](lv_timer_t* t){
        auto *self = static_cast<CommandersMissionPage*>(t->user_data);
        lv_async_call([](void* user_data) 
        {
            auto *me = static_cast<CommandersMissionPage*>(user_data);
            if(me->tileZoom == 0)
            {
                return;
            }

            JsonDocument currentCommandersEvent;
            float lat, lon;
            if(me->fakeGPS)
            {
                LVGLPage::generateNearbyCoordinatesFromTile(me->tileX, me->tileY, me->tileZoom, lat, lon);
            }
            else
            {
                lat = me->gpsModule->getLat();
                lon = me->gpsModule->getLon();
            }
            
            me->commanderModule->updateReceivedData(me->commanderModule->getCommanderNumber(), lat, lon);

            currentCommandersEvent["time_sent"] = CommandersMissionPage::getCurrentTimeStamp().c_str();

            currentCommandersEvent["latitude"] = lat;
            currentCommandersEvent["longitude"] = lon;
            currentCommandersEvent["heartRate"] = me->generateHeartRate();
            currentCommandersEvent["soldierId"] = me->commanderModule->getName();

            FFatHelper::appendRegularJsonObjectToFile(me->logFilePath.c_str(), currentCommandersEvent);

        }, self);

    }, 10000, this);

    Serial.println("this->missingSoldierTimer");
    this->missingSoldierTimer = lv_timer_create([](lv_timer_t* t){
        auto *self = static_cast<CommandersMissionPage*>(t->user_data);
        if(self->commanderSwitchEvent || !self->initialCoordsReceived)
        {
            return;
        }
        for (const auto& commander : self->commanderModule->getCommanders()) 
        {
            if(commander.first != self->commanderModule->getCommanderNumber() && millis() - commander.second.lastTimeReceivedData >= 60000)
            {
                self->missingSoldierEvent(commander.first, true);
                return;
            }
        }

        for (const auto& soldier : self->commanderModule->getSoldiers()) 
        {
            if(millis() - soldier.second.lastTimeReceivedData >= 60000)
            {
                self->missingSoldierEvent(soldier.first, false);
                return;
            }
        }
    }, 60000, this);
}

void CommandersMissionPage::onDataReceived(const uint8_t* data, size_t len)
{
    Serial.printf("onDataReceived %d\n", len);
    String incoming;
    for (size_t i = 0; i < len; i++) 
    {
        incoming += (char)data[i];
    }
    Serial.println("onDataReceived");

    Serial.printf("-------------------%d % \n------------------------", watch.getBatteryPercent());

    int p1 = incoming.indexOf('|');
    int p2 = incoming.indexOf('|', p1 + 1);
    if (p1 < 0 || p2 < 0) {                      
        Serial.println("Bad ciphertext format");
        return;
    }
    Serial.println("Bad ciphertext format123");
    crypto::Ciphertext ct;
    ct.nonce = crypto::CryptoModule::hexToBytes(incoming.substring(0, p1));
    ct.data = crypto::CryptoModule::hexToBytes(incoming.substring(p1 + 1, p2));
    ct.tag = crypto::CryptoModule::hexToBytes(incoming.substring(p2 + 1));

    crypto::ByteVec pt;

    try {
        pt = crypto::CryptoModule::decrypt(this->commanderModule->getGK(), ct);
    } catch (const std::exception& e)
    {
        Serial.printf("Decryption failed: %s\n", e.what());
        try
        {
            pt = crypto::CryptoModule::decrypt(this->commanderModule->getCompGMK(), ct);
        }
        catch(const std::exception& e)
        {
            Serial.printf("Comp Decryption failed: %s\n", e.what());
            return;
        }
        
    }

    SoldiersSentData* newG = reinterpret_cast<SoldiersSentData*>(pt.data());
    String plainStr;
    plainStr.reserve(pt.size());

    for (unsigned char b : pt)
    {
         plainStr += (char)b;
    }

    float tile_lat = newG->tileLat;
    float tile_lon = newG->tileLon;
    float marker_lat = newG->posLat;
    float marker_lon = newG->posLon;

    Serial.printf("%.5f %.5f %.5f %.5f\n", tile_lat, tile_lon, marker_lat, marker_lon);

    if(isZero(tile_lat) || isZero(tile_lon) || isZero(marker_lat) || isZero(marker_lon))
    {
        return;
    }
    Serial.println("isZero");
    String timeStr = CommandersMissionPage::getCurrentTimeStamp();

    Serial.println("getLocalTime");

    JsonDocument currentEvent;
    currentEvent["time_sent"] = timeStr.c_str();
    currentEvent["latitude"] = marker_lat;
    currentEvent["longitude"] = marker_lon;
    currentEvent["heartRate"] = newG->heartRate;
    
    Serial.printf("Current Event: %d\n", newG->heartRate);
    Serial.printf("Current id: %d\n", newG->soldiersID);
    String name;

    if(this->commanderModule->getCommanders().find(newG->soldiersID) != this->commanderModule->getCommanders().end())
    {
        name = String(this->commanderModule->getCommanders().at(newG->soldiersID).name.c_str());
    }
    else
    {
        name = String(this->commanderModule->getSoldiers().at(newG->soldiersID).name.c_str());
    }

    Serial.printf("Name is: %s\n", name.c_str());
    currentEvent["soldierId"] = name;

    bool writeResult = FFatHelper::appendRegularJsonObjectToFile(this->logFilePath.c_str(), currentEvent);
    Serial.print("writeToFile result: ");
    Serial.println(writeResult ? "success" : "failure");

    Serial.println(this->logFilePath.c_str());

    if(this->tileZoom == 0)
    {
        Serial.println("positionToTile");

        std::tuple<int,int,int> tileLocation = positionToTile(tile_lat, tile_lon, 19);

        this->tileZoom = std::get<0>(tileLocation);
        this->tileX = std::get<1>(tileLocation);
        this->tileY = std::get<2>(tileLocation);
    }
    

    if(!FFatHelper::isFileExisting(this->tileFilePath))
    {
        std::string middleTile = this->tileUrlInitial +
        std::to_string(this->tileZoom) + "/" +
        std::to_string(this->tileX) + "/" +
        std::to_string(this->tileY) + ".png";


        Serial.println(middleTile.c_str());
        this->wifiModule->downloadFile(middleTile.c_str(), this->tileFilePath);
    }

    FFatHelper::listFiles();
 
    Serial.println("downloadFile");
    Serial.println(this->markers.empty());
    for (const auto& kv : this->markers) {
        Serial.println(kv.first);
    }

    if(this->markers.empty())
    {
        showMiddleTile();
        this->initialCoordsReceived = true;
        this->commanderModule->resetAllData();

    }

    auto [centerLat, centerLon] = tileCenterLatLon(this->tileZoom, this->tileX, this->tileY);

    int heartRate = newG->heartRate;
    lv_color_t color = getColorFromHeartRate(heartRate);

    ballColors[newG->soldiersID] = color;
    Serial.println(ballColors[newG->soldiersID].full);

    lv_obj_t* marker = nullptr;
    lv_obj_t* label = nullptr;

    if (markers.find(newG->soldiersID) != markers.end()) {
        marker = markers[newG->soldiersID];
    }
    if (labels.find(newG->soldiersID) != labels.end()) {
        label = labels[newG->soldiersID];
    }

    create_fading_circle(marker_lat, marker_lon, centerLat, centerLon, newG->soldiersID, 19, &ballColors[newG->soldiersID], marker, label);

    this->commanderModule->updateReceivedData(newG->soldiersID, marker_lat, marker_lon);
    const std::vector<uint8_t>& comp = this->commanderModule->getComp();

    if(newG->heartRate == 0 && std::find(comp.begin(), comp.end(), newG->soldiersID) == comp.end())
    {
        Serial.println("CompromisedEvent");
        this->commanderModule->setCompromised(newG->soldiersID);

        const auto& commanders = this->commanderModule->getCommanders();
        const auto& soldiers = this->commanderModule->getSoldiers();

        std::string who;
        if (auto it = commanders.find(newG->soldiersID); it != commanders.end()) 
        {
            who = it->second.name;
        } else if (auto it2 = soldiers.find(newG->soldiersID); it2 != soldiers.end()) 
        {
            who = it2->second.name;
        } else 
        {
            who = "ID " + std::to_string(newG->soldiersID);
        }

        delay(500);
        const std::string newEventText = "Compromised Soldier Event - " + who;
        this->switchGMKEvent(newEventText.c_str(), newG->soldiersID);
        
        JsonDocument doc;
        doc["timestamp"] = CommandersMissionPage::getCurrentTimeStamp().c_str();
        doc["eventName"] = "compromisedSoldier";
        doc["compromisedID"] = newG->soldiersID;

        FFatHelper::appendJSONEvent(this->logFilePath.c_str(), doc);

        delay(10);

        doc.clear();
        doc["timestamp"] = CommandersMissionPage::getCurrentTimeStamp().c_str();
        doc["eventName"] = "SOS";
        doc["SOSID"] = newG->soldiersID;

        FFatHelper::appendJSONEvent(this->logFilePath.c_str(), doc);

        return;
    }

}

inline bool CommandersMissionPage::isZero(float x)
{
    return std::fabs(x) < 1e-9f;
}

std::tuple<int, int, int> CommandersMissionPage::positionToTile(float lat, float lon, int zoom)
{
    float lat_rad = radians(lat);
    float n = pow(2.0, zoom);
    
    int x_tile = floor((lon + 180.0) / 360.0 * n);
    int y_tile = floor((1.0 - log(tan(lat_rad) + 1.0 / cos(lat_rad)) / PI) / 2.0 * n);
    
    return std::make_tuple(zoom, x_tile, y_tile);
}

void CommandersMissionPage::showMiddleTile() 
{
    Serial.println("showMiddleTile");

    this->tileImg = lv_img_create(lv_scr_act());
    lv_obj_center(this->tileImg);

    lv_img_set_src(this->tileImg, "A:/middleTile.png");
    lv_obj_align(this->tileImg, LV_ALIGN_CENTER, 0, 0);

    Serial.println("Showing middle tile");
}

std::pair<double,double> CommandersMissionPage::tileCenterLatLon(int zoom, int x_tile, int y_tile)
{
    static constexpr double TILE_SIZE = 256.0;

    int centerX = x_tile * TILE_SIZE + TILE_SIZE / 2;
    int centerY = y_tile * TILE_SIZE + TILE_SIZE / 2;
    
    double n = std::pow(2.0, zoom);
    double lon_deg = (double)centerX / (n * TILE_SIZE) * 360.0 - 180.0;
    double lat_rad = std::atan(std::sinh(M_PI * (1 - 2.0 * centerY / (n * TILE_SIZE))));
    double lat_deg = lat_rad * 180.0 / M_PI;
    
    return {lat_deg, lon_deg};
}

void CommandersMissionPage::create_fading_circle(double markerLat, double markerLon, double centerLat, double centerLon, uint16_t soldiersID, int zoom,
     lv_color_t* ballColor, lv_obj_t*& marker, lv_obj_t*& label) 
{
    Serial.println(marker != NULL);

    if (marker != NULL) {
        lv_obj_del(marker);
        marker = NULL;
    }


    auto [pixel_x, pixel_y] = latlon_to_pixel(markerLat, markerLon, centerLat, centerLon, zoom);


    marker = lv_obj_create(lv_scr_act());
    lv_obj_set_size(marker, 10, 10);
    lv_obj_set_style_bg_color(marker, *ballColor, 0);
    lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, 0);


    lv_obj_set_pos(marker, pixel_x, pixel_y);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, marker);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_playback_time(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, [](void * obj, int32_t value) {
        lv_obj_set_style_opa((lv_obj_t *)obj, value, 0);
    });

    lv_anim_start(&a);
    Serial.printf("Set animation for %d, in %d %d\n", soldiersID, pixel_x, pixel_y);

    if(!label)
    {
        
        label = lv_label_create(lv_scr_act());

        const std::unordered_map<uint8_t, CommanderInfo>& commanders = this->commanderModule->getCommanders();
        const std::unordered_map<uint8_t, SoldierInfo>& soldiers = this->commanderModule->getSoldiers();

        for (const auto& kv : commanders) {
            Serial.printf("Other Commander ID: %u\n", kv.first);
        }

        for (const auto& kv : soldiers) {
            Serial.printf("Other Soldier ID: %u\n", kv.first);
        }

        auto commandIt = commanders.find(soldiersID);
        auto soldIt = soldiers.find(soldiersID);

        if (commandIt != commanders.end()) 
        {
            Serial.printf("Commander %u found in map!!\n", soldiersID);
            std::string s = commandIt->second.name;
            lv_label_set_text(label, s.c_str());
        } 
        else if(soldIt != soldiers.end())
        {
            Serial.printf("Soldier %u found in map!!\n", soldiersID);
            std::string s = soldIt->second.name;
            lv_label_set_text(label, s.c_str());
        }
        else
        {
            Serial.printf("Soldier %u not found in map\n", soldiersID);
        }

    }
    
    lv_obj_align_to(label, marker, LV_ALIGN_TOP_MID, 0, -35);

    markers[soldiersID] = marker;
    labels[soldiersID] = label;
}

void CommandersMissionPage::setTransferFunction(std::function<void(std::shared_ptr<LoraModule>, std::unique_ptr<WifiModule>,
         std::shared_ptr<GPSModule>, std::unique_ptr<FHFModule>, std::unique_ptr<Soldier>)> cb)
{
    this->transferFunction = cb;
}


String CommandersMissionPage::getCurrentTimeStamp()
{
    struct tm timeInfo;
    struct timeval tv;

    char timeStr[40];
    if (gettimeofday(&tv, NULL) == 0)
    {
        gmtime_r(&tv.tv_sec, &timeInfo);
        snprintf(timeStr, sizeof(timeStr),
                 "%04d-%02d-%02dT%02d:%02d:%02d.%06ldZ",
                 timeInfo.tm_year + 1900,
                 timeInfo.tm_mon + 1,
                 timeInfo.tm_mday,
                 timeInfo.tm_hour,
                 timeInfo.tm_min,
                 timeInfo.tm_sec,
                 tv.tv_usec);
    }
    else
        strcpy(timeStr, "1970-01-01T00:00:00.000000Z");

    return timeStr;
}

std::tuple<int,int> CommandersMissionPage::latlon_to_pixel(double lat, double lon,
                                                           double centerLat, double centerLon,
                                                           int zoom) {
    int imgX = lv_obj_get_x(this->tileImg);
    int imgY = lv_obj_get_y(this->tileImg);
    int imgW = lv_obj_get_width(this->tileImg);
    int imgH = lv_obj_get_height(this->tileImg);

    auto toGlobal = [&](double la, double lo) {
        double rad = la * M_PI / 180.0;
        double n   = std::pow(2.0, zoom);
        int xg = (int) std::floor((lo + 180.0) / 360.0 * n * 256.0);
        int yg = (int) std::floor((1.0 - std::log(std::tan(rad) + 1.0 / std::cos(rad)) / M_PI) / 2.0 * n * 256.0);
        return std::make_pair(xg, yg);
    };

    auto [markerX, markerY] = toGlobal(lat, lon);
    auto [centerX, centerY] = toGlobal(centerLat, centerLon);

    int localX = (markerX - centerX) + (imgX + imgW / 2);
    int localY = (markerY - centerY) + (imgY + imgH / 2);

    return {localX, localY};
}

void CommandersMissionPage::switchGMKEvent(const char* infoBoxText, uint8_t soldiersIDMoveToComp)
{
    Serial.println("switchGMKEvent");
    JsonDocument switchGKDoc;
    switchGKDoc["msgID"] = 0x08;
    String switchGKJson;

    serializeJson(switchGKDoc, switchGKJson);
    crypto::ByteVec payload(switchGKJson.begin(), switchGKJson.end());
    crypto::Ciphertext ct = crypto::CryptoModule::encrypt(this->commanderModule->getGK(), payload);

    String msg = crypto::CryptoModule::encodeCipherText(ct);

    for(uint8_t i = 0; i < 4; ++i)
    {
        this->loraModule->cancelReceive();
        this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());

        delay(100);
    }

    bool prevCommanderSwitchEvent = this->commanderSwitchEvent;

    this->commanderSwitchEvent = true;
    
    payload.clear();
    switchGKJson.clear();
    switchGKDoc.clear();
    msg.clear();

    switchGKDoc["msgID"] = 0x01;
    switchGKDoc["soldiersID"] = soldiersIDMoveToComp;
    
    Serial.println("IMPORTANT INFO");
    std::string info(this->commanderModule->getName());
    crypto::ByteVec salt(32);
    randombytes_buf(salt.data(), salt.size());
    
    switchGKDoc["info"] = info;
    switchGKDoc["millis"] = millis();
    
    const crypto::Key256 newGMK = crypto::CryptoModule::deriveGK(this->commanderModule->getGMK(), switchGKDoc["millis"].as<uint64_t>(), info, salt, this->commanderModule->getCommanderNumber());
    Serial.printf("NEW GMK: %s\n", crypto::CryptoModule::keyToHex(newGMK).c_str());
    

    std::unordered_map<uint8_t, bool> allSoldiers;

    for (const auto& [k, v] : this->commanderModule->getCommanders()) 
    {
        if(!this->commanderModule->isComp(k) || k == soldiersIDMoveToComp)
        {
            allSoldiers[k] = false;
        }
    }

    for (const auto& [k, v] : this->commanderModule->getSoldiers()) 
    {
        if(!this->commanderModule->isComp(k) || k == soldiersIDMoveToComp)
        {
            allSoldiers[k] = true;
        }
    }

    crypto::Key256 saltAsKey;
    std::copy(salt.begin(), salt.end(), saltAsKey.begin());
    

    const std::string saltRaw(reinterpret_cast<const char*>(saltAsKey.data()), saltAsKey.size());

    const crypto::Key256& compSalt = this->commanderModule->getCompSalt();
    const std::string compSaltRaw(reinterpret_cast<const char*>(compSalt.data()), compSalt.size());

    const crypto::Key256 newCompGK = crypto::CryptoModule::deriveGK(this->commanderModule->getGMK(), this->commanderModule->getCompMillis(), this->commanderModule->getCompInfo(),
                                   crypto::ByteVec(compSaltRaw.begin(), compSaltRaw.end()),
                                   this->commanderModule->getCommanderNumber());

    Serial.printf("NEW COMPGMK: %s\n", crypto::CryptoModule::keyToHex(newCompGK).c_str());

    for (const auto& soldier : allSoldiers) 
    {
        switchGKJson.clear();

        if(soldier.first == this->commanderModule->getCommanderNumber())
        {
            continue;
        }

        if(soldier.first == soldiersIDMoveToComp)
        {
            switchGKDoc["info"].clear();
            switchGKDoc["info"] = this->commanderModule->getCompInfo();
            switchGKDoc["millis"] = this->commanderModule->getCompMillis();
        }

        const std::string& saltToSendRaw = soldier.first == soldiersIDMoveToComp ? compSaltRaw : saltRaw;
                
        switchGKDoc["soldiersID"] = soldier.first;
        Serial.printf("%d\n", switchGKDoc["soldiersID"]);
        crypto::ByteVec saltCipher;

        if(soldier.second)
        {
            certModule::encryptWithPublicKey(this->commanderModule->getSoldiers().at(switchGKDoc["soldiersID"].as<uint8_t>()).cert,
            saltToSendRaw, saltCipher);
        }
        else
        {
            certModule::encryptWithPublicKey(this->commanderModule->getCommanders().at(switchGKDoc["soldiersID"].as<uint8_t>()).cert,
            saltToSendRaw, saltCipher);
        }

        switchGKDoc.remove("salt");
        JsonArray sarr = switchGKDoc.createNestedArray("salt");
        for (uint8_t b : saltCipher) sarr.add(b);

        serializeJson(switchGKDoc, switchGKJson);
        Serial.println("crypto::CryptoModule::key256ToAsciiString");

        crypto::ByteVec payload(switchGKJson.begin(), switchGKJson.end());
        crypto::Ciphertext ct = crypto::CryptoModule::encrypt(this->commanderModule->getGK(), payload);

        String msg;
        msg = crypto::CryptoModule::encodeCipherText(ct);
        Serial.printf("SALT SENT: %s\n", crypto::CryptoModule::key256ToAsciiString(saltAsKey).c_str());

        Serial.printf("PAYLOAD SENT (base64): %s %d\n", switchGKJson.c_str(), msg.length());

        Serial.println("SENDING base64Payload");
        this->loraModule->cancelReceive();
        this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());

        delay(500);
    }
    
    this->commanderSwitchEvent = prevCommanderSwitchEvent;

    this->commanderModule->setGK(newGMK);
    this->commanderModule->setCompGMK(newCompGK);

    LVGLPage::restartInfoBoxFadeout(this->infoBox, 1000, 5000, infoBoxText);
}

void CommandersMissionPage::missingSoldierEvent(uint8_t soldiersID, bool isCommander)
{
    Serial.printf("missingSoldierEvent for %d\n", soldiersID);

    std::string newEventText;
    String missingName = "";

    if(isCommander)
    {
        missingName = String(this->commanderModule->getCommanders().at(soldiersID).name.c_str());
    }
    else
    {
        missingName = String(this->commanderModule->getSoldiers().at(soldiersID).name.c_str());
        
    }

    newEventText = "Missing Soldier Event - " + std::string(missingName.c_str());
    Serial.println("PRE PREMOVE");

    this->commanderModule->setMissing(soldiersID);

    this->switchGMKEvent(newEventText.c_str());

    JsonDocument doc;
    doc["timestamp"] = CommandersMissionPage::getCurrentTimeStamp().c_str();
    doc["eventName"] = "missingSoldier";
    doc["missingID"] = missingName;

    FFatHelper::appendJSONEvent(this->logFilePath.c_str(), doc);
}

void CommandersMissionPage::switchCommanderEvent()
{
    this->commanderSwitchEvent = true;
    this->loraModule->setOnReadData(nullptr);

    Serial.println("switchCommanderEvent");

    //Split here the log file, its path is this->logFilePath
    std::vector<String> sharePaths;

    lv_timer_del(this->missingSoldierTimer);
    lv_timer_del(this->selfLogTimer);
    
    this->missingSoldierTimer = nullptr;
    this->selfLogTimer = nullptr;

    JsonDocument switchCommanderDoc;
    switchCommanderDoc["msgID"] = 0x08;
    String switchCommanderJson;

    serializeJson(switchCommanderDoc, switchCommanderJson);
    crypto::ByteVec payload(switchCommanderJson.begin(), switchCommanderJson.end());
    crypto::Ciphertext ct = crypto::CryptoModule::encrypt(this->commanderModule->getGK(), payload);

    String msg = crypto::CryptoModule::encodeCipherText(ct);
    
    for(uint8_t i = 0; i < 4; ++i)
    {
        this->loraModule->cancelReceive();
        this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());

        delay(100);
    }

    switchCommanderJson.clear();
    switchCommanderDoc.clear();
    payload.clear();
    msg.clear();
    
    uint8_t nextID = -1;
    String newCommandersName = "";
    if(this->commanderModule->getCommandersInsertionOrder().size() > 1)
    {
        nextID = this->commanderModule->getCommandersInsertionOrder().at(1);
        newCommandersName = String(this->commanderModule->getCommanders().at(nextID).name.c_str());
    }

    switchCommanderDoc["msgID"] = 0x02;

    JsonDocument doc;
    doc["timestamp"] = CommandersMissionPage::getCurrentTimeStamp().c_str();
    doc["eventName"] = "commanderSwitch";
    doc["newCommanderID"] = newCommandersName;

    FFatHelper::appendJSONEvent(this->logFilePath.c_str(), doc);

    uint8_t index = 0;

    std::vector<uint8_t> allSoldiers;
    allSoldiers.reserve(this->commanderModule->getCommanders().size() + this->commanderModule->getSoldiers().size());

    std::vector<std::pair<float,float>> soldiersCoords;
    std::vector<uint8_t> soldiersCoordsIDS;

    for (const auto& [k, v] : this->commanderModule->getCommanders()) 
    {
        if(!this->commanderModule->isComp(k))
        {
            allSoldiers.push_back(k);
        }

        if(this->commanderModule->areCoordsValid(k, false))
        {
            soldiersCoords.emplace_back(this->commanderModule->getLocation(k, false));
            soldiersCoordsIDS.emplace_back(k);

            Serial.printf("Pushing back {%.5f %.5f} %d\n", this->commanderModule->getLocation(k, false).first, this->commanderModule->getLocation(k, false).second, k);
        }
        
    }

    for (const auto& [k, v] : this->commanderModule->getSoldiers()) 
    {
        if(!this->commanderModule->isComp(k))
        {
            allSoldiers.push_back(k);
        }


        if(this->commanderModule->areCoordsValid(k, true))
        {
            soldiersCoords.emplace_back(this->commanderModule->getLocation(k, true));
            soldiersCoordsIDS.emplace_back(k);

            Serial.printf("Pushing back {%.5f %.5f} %d\n", this->commanderModule->getLocation(k, true).first, this->commanderModule->getLocation(k, true).second, k);
        }
    }

    for(uint8_t idx = 0; idx < soldiersCoords.size(); ++idx)
    {
        Serial.printf("ID and coord commander -> %d { %.3f %.3f }\n", soldiersCoordsIDS.at(idx), soldiersCoords.at(idx).first, soldiersCoords.at(idx).second);
    }

    Serial.println("allSoldiers:");
    for(const auto& sold : allSoldiers)
    {
        Serial.println(sold);
    }

    if (!ShamirHelper::splitFile(this->logFilePath.c_str(), allSoldiers.size(), 
    allSoldiers, sharePaths)) 
    {
        Serial.println("❌ Failed to split log file with Shamir.");
        return;
    }

    Serial.println("sharePaths:");
    for(const auto& path : sharePaths)
    {
        Serial.println(path.c_str());
    }
    if(allSoldiers.size() > 1)
    {
        LVGLPage::restartInfoBoxFadeout(this->infoBox, 1000, 5000, "Sending Shamir!");
    }


    for (const auto& soldier : allSoldiers) 
    {
        if(soldier == this->commanderModule->getCommanderNumber())
        {
            index++;
            continue;
        }

        msg.clear();
        payload.clear();
        switchCommanderDoc.clear();
        switchCommanderJson.clear();

        Serial.printf("Sending to soldier %d\n", soldier);
        std::vector<uint16_t> shamirPart;

        switchCommanderDoc["msgID"] = 0x02;
        switchCommanderDoc["soldiersID"] = soldier;

        JsonArray comp = switchCommanderDoc.createNestedArray("compromisedSoldiers");
        for (uint8_t id : this->commanderModule->getComp()) comp.add(id);

        JsonArray miss = switchCommanderDoc.createNestedArray("missingSoldiers");
        for (uint8_t id : this->commanderModule->getMissing()) miss.add(id);

        JsonArray ids = switchCommanderDoc.createNestedArray("soldiersCoordsIDS");
        for (uint8_t id : soldiersCoordsIDS) ids.add(id);

        JsonArray coords = switchCommanderDoc.createNestedArray("soldiersCoords");
        for (const auto& p : soldiersCoords) 
        {
            JsonArray row = coords.createNestedArray();
            row.add(p.first);
            row.add(p.second);
        }

        JsonArray parts = switchCommanderDoc.createNestedArray("shamirPart");

        File currentShamir = FFat.open(sharePaths[index].c_str(), FILE_READ);
        if (!currentShamir) 
        {
            Serial.print("❌ Failed to open share file: ");
            Serial.println(sharePaths[index]);
            continue;
        }

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

        FFatHelper::deleteFile(sharePaths[index].c_str());

        for (const auto& pt : shamirPart) 
        {
            parts.add(pt);
        }

        serializeJson(switchCommanderDoc, switchCommanderJson);
        payload.assign(switchCommanderJson.begin(), switchCommanderJson.end());

        Serial.println(switchCommanderJson.c_str());
        Serial.println("SENDING base64Payload");
        
        ct = crypto::CryptoModule::encrypt(this->commanderModule->getGK(), payload);
        msg = crypto::CryptoModule::encodeCipherText(ct);

        this->loraModule->cancelReceive();
        this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());
    
        ++index;

        delay(3000);
    }

    delay(5000);

    switchCommanderDoc.clear();
    switchCommanderJson.clear();
    msg.clear();
    payload.clear();

    switchCommanderDoc["msgID"] = 0x07;
    serializeJson(switchCommanderDoc, switchCommanderJson);
    payload.assign(switchCommanderJson.begin(), switchCommanderJson.end());

    ct = crypto::CryptoModule::encrypt(this->commanderModule->getGK(), payload);
    msg = crypto::CryptoModule::encodeCipherText(ct);

    this->loraModule->cancelReceive();
    this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());

    delay(1000);

    this->finishMainTimer = true;

    if(this->regularLoopTimer)
    {
        lv_timer_del(this->regularLoopTimer);
        this->regularLoopTimer = nullptr;
    }

    Serial.println("finished loop");

    Serial.println("deleted timer");

    this->commanderModule->removeFirstCommanderFromInsertionOrder();

    std::unique_ptr<Soldier> sold = std::make_unique<Soldier>(this->commanderModule->getName(),
     this->commanderModule->getPublicCert(), this->commanderModule->getPrivateKey(),
     this->commanderModule->getCAPublicCert(), this->commanderModule->getCommanderNumber(),
     this->commanderModule->getIntervalMS());

    sold->setGMK(this->commanderModule->getGMK());
    sold->setGK(this->commanderModule->getGK());
    sold->setInsertionOrders(this->commanderModule->getCommandersInsertionOrder());
    sold->setCommanders(this->commanderModule->getCommanders());

    Serial.println("created sold");
    this->destroyPage();
    delay(10);

    Serial.println("destroyPage");

    this->ballColors.clear();
    this->markers.clear();
    this->labels.clear();

    Serial.println("this->transferFunction");

    if (c_pmuOwner == this)
    {
         c_pmuOwner = nullptr;
    }
    
    watch.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
    watch.clearPMU();

    this->transferFunction(this->loraModule, std::move(this->wifiModule),
     this->gpsModule, std::move(this->fhfModule), std::move(sold));
}

void CommandersMissionPage::onFinishedSplittingShamirReceived(const uint8_t* data, size_t len)
{
    Serial.println("onFinishedSplittingShamirReceived");

    crypto::Ciphertext ct = crypto::CryptoModule::decodeText(data, len);
    crypto::ByteVec pt;

    try {
        pt = crypto::CryptoModule::decrypt(this->commanderModule->getGK(), ct);
    } catch (const std::exception& e)
    {
        Serial.printf("Decryption failed: %s\n", e.what());
        return;        
    }

    JsonDocument finishSplittingReceivedDoc;
    DeserializationError e = deserializeJson(finishSplittingReceivedDoc, String((const char*)pt.data(), pt.size()));
    if (e) { Serial.println("coords JSON parse failed"); return; }
    
    uint8_t msgID = finishSplittingReceivedDoc["msgID"].as<uint8_t>();

    if(msgID != 0x07)
    {
        Serial.printf("ID is not 0x07! %d\n", msgID);
        return;
    }

    JsonDocument sendShamirDoc;
    sendShamirDoc["msgID"] = 0x03;
    sendShamirDoc["soldiersID"] = this->commanderModule->getCommanderNumber();

    char sharePath[25];
    snprintf(sharePath, sizeof(sharePath), "/share_%u.txt", this->commanderModule->getCommanderNumber());
    Serial.println(sharePath);

    File currentShamir = FFat.open(sharePath, FILE_READ);
    if (!currentShamir) 
    {
        Serial.print("❌ Failed to open share file: ");
        Serial.println(sharePath);

        return;
    }

    JsonArray parts = sendShamirDoc.createNestedArray("shamirPart");
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

    String sendShamirJson;
    serializeJson(sendShamirDoc, sendShamirJson);

    pt.clear();
    pt.assign(sendShamirJson.begin(), sendShamirJson.end());

    ct = crypto::CryptoModule::encrypt(this->commanderModule->getGK(), pt);

    String msg = crypto::CryptoModule::encodeCipherText(ct);

    delay(5000);

    this->onShamirPartReceived(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());
}

void CommandersMissionPage::onShamirPartReceived(const uint8_t* data, size_t len)
{
    Serial.println("onShamirPartReceived");
    crypto::Ciphertext ct = crypto::CryptoModule::decodeText(data, len);
    crypto::ByteVec pt;

    try 
    {
        pt = crypto::CryptoModule::decrypt(this->commanderModule->getGK(), ct);
    } catch (const std::exception& e)
    {
        Serial.printf("Decryption failed: %s\n", e.what());
        return;
        
    }

    JsonDocument shamirReceivedDoc;
    DeserializationError e = deserializeJson(shamirReceivedDoc, String((const char*)pt.data(), pt.size()));
    if (e) { Serial.println("coords JSON parse failed"); return; }

    if (shamirReceivedDoc.size() < 2) {
        Serial.println("Decoded data too short for any struct with length prefix");
        return;
    }


    JsonDocument sendShamirDoc;

    uint8_t msgID = shamirReceivedDoc["msgID"].as<uint8_t>();
    uint8_t soldiersID = shamirReceivedDoc["soldiersID"].as<uint8_t>();

    sendShamirDoc["msgID"] = msgID;
    sendShamirDoc["soldiersID"] = soldiersID;

    if(msgID != 0x03)
    {
        Serial.printf("ID is not 0x03! %d\n", msgID);
        return;
    }

    size_t idx = 2;

    JsonArray shamirPartArr = shamirReceivedDoc["shamirPart"].as<JsonArray>();

    Serial.printf("Deserialized sendShamir: msgID=%d, soldiersID=%d, shamirPart size=%zu\n",
                  msgID, soldiersID, shamirPartArr.size());

    if(shamirPartArr.size() == 0)
    {
        return;
    }

    char sharePath[25];
    snprintf(sharePath, sizeof(sharePath), "/share_%u.txt", shamirReceivedDoc["soldiersID"].as<uint8_t>());
    Serial.println(sharePath);

    if(!FFat.exists(sharePath))
    {
        File currentShare = FFat.open(sharePath, FILE_WRITE);
        if (!currentShare)
        {
            Serial.print("Failed to open file to save share: ");
            Serial.println(sharePath);
            return;
        }

        uint16_t x = soldiersID;

        for (JsonVariant v : shamirPartArr) 
        {
            uint16_t y = v.as<uint16_t>();
            currentShare.printf("%u,%u\n", x, y);
        }


        this->shamirPartsCollected++;
        this->didntSendShamir.erase(this->didntSendShamir.begin());

        currentShare.close();
        this->shamirPaths.push_back(sharePath);
    }

    if(this->shamirPartsCollected >= ShamirHelper::getThreshold())
    {
        Serial.println("ShamirHelper::getThreshold()");
        Serial.println("this->shamirPaths:");

        for(const auto& path: this->shamirPaths)
        {
            Serial.println(path);
        }

        ShamirHelper::reconstructFile(this->shamirPaths, this->logFilePath.c_str());

        String content;
        FFatHelper::readFile(this->logFilePath.c_str(), content);
        
        Serial.println(content);

        FFatHelper::removeFilesIncludeWords("share", "txt");
        
        JsonDocument commanderInitializedDoc;
        commanderInitializedDoc["msgID"] = 0x06;
        String commanderInitalizedJson;

        serializeJson(commanderInitializedDoc, commanderInitalizedJson);
        crypto::ByteVec payload(commanderInitalizedJson.begin(), commanderInitalizedJson.end());
        ct = crypto::CryptoModule::encrypt(this->commanderModule->getGK(), payload);

        String msg = crypto::CryptoModule::encodeCipherText(ct);
            
        this->loraModule->cancelReceive();
        this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());
        
        JsonDocument finializedCommanderEvent;
        finializedCommanderEvent["timestamp"] = CommandersMissionPage::getCurrentTimeStamp().c_str();
        finializedCommanderEvent["eventName"] = "finalizedCommander";
        finializedCommanderEvent["finalizedID"] = this->commanderModule->getCommanderNumber();

        FFatHelper::appendJSONEvent(this->logFilePath.c_str(), finializedCommanderEvent);
        
        delay(2000);


        this->commanderSwitchEvent = false;

        this->commanderModule->resetAllData();

        this->loraModule->setOnReadData([this](const uint8_t* data, size_t len) {
                this->onDataReceived(data, len);
        });

        this->loraModule->setOnFileReceived(nullptr);

        this->didntSendShamir.clear();

        return;
    } 

    if (this->shamirPartsCollected == 1 && !this->initialShamirReceived)
    {
        this->initialShamirReceived = true;

        Serial.println("!this->loraModule->getOnFileReceived()");
        this->loraModule->setOnFileReceived([this](const uint8_t* data, size_t len) {
            this->onShamirPartReceived(data, len);
        });

        this->loraModule->setOnReadData([this](const uint8_t* data, size_t len) {
            this->loraModule->onLoraFileDataReceived(data, len);
        });

        
    }

    this->sendNextShamirRequest();
    
}

void CommandersMissionPage::sendNextShamirRequest()
{

    if (this->didntSendShamir.empty())
    {
        return;
    }

    this->currentShamirRec = this->didntSendShamir.front();

    JsonDocument requestShamirDoc;
    requestShamirDoc["msgID"] = 0x04;
    requestShamirDoc["soldiersID"] = this->currentShamirRec;

    String requestShamirJson;
    serializeJson(requestShamirDoc, requestShamirJson);

    crypto::ByteVec payload(requestShamirJson.begin(), requestShamirJson.end());
    crypto::Ciphertext ct = crypto::CryptoModule::encrypt(this->commanderModule->getGK(), payload);

    String msg = crypto::CryptoModule::encodeCipherText(ct);

    this->loraModule->cancelReceive();
    this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(msg.c_str()), msg.length());


    shamirTimeoutTimer = lv_timer_create(
        [](lv_timer_t* t) {
            auto *self = static_cast<CommandersMissionPage*>(t->user_data);
            if(self->loraModule->isCurrentlyReadingFile())
            {
                Serial.println("Currently reading file ! !!");
                return;
            }
            if (!self->didntSendShamir.empty())
            {
                Serial.printf("shamirTimeoutTimer started for %d!\n", self->currentShamirRec);
            }
            else
            {
                Serial.println("shamirTimeoutTimer started !");
            }
            

            lv_timer_del(t);
            self->shamirTimeoutTimer = nullptr;


            if (!self->didntSendShamir.empty() && self->didntSendShamir.front() == self->currentShamirRec)
            {
                self->didntSendShamir.erase(self->didntSendShamir.begin());

                if(self->shamirPartsCollected < ShamirHelper::getThreshold())
                {
                    self->sendNextShamirRequest();
                }
                
            }

            Serial.println("shamirTimeoutTimer finished!");
        },
        10000,
        this
    );
}