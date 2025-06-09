#include "CommandersMissionPage.h"

CommandersMissionPage::CommandersMissionPage(std::shared_ptr<LoraModule> loraModule, std::unique_ptr<WifiModule> wifiModule,
     std::shared_ptr<GPSModule> gpsModule, std::unique_ptr<FHFModule> fhfModule,
    std::unique_ptr<Commander> commanderModule, const std::string& logFilePath, bool commanderChange, bool fakeGPS)
    : logFilePath(logFilePath) 
    {
        this->loraModule = std::move(loraModule);
        this->wifiModule = std::move(wifiModule);
        this->gpsModule = std::move(gpsModule);
        this->fhfModule = std::move(fhfModule);
        this->commanderModule = std::move(commanderModule);

        this->commanderSwitchEvent = commanderChange;

        Serial.printf("🔍 Checking modules for %d:\n", this->commanderModule->getCommanderNumber());
        Serial.printf("📡 loraModule: %s\n", this->loraModule ? "✅ OK" : "❌ NULL");
        Serial.printf("🌐 wifiModule: %s\n", this->wifiModule ? "✅ OK" : "❌ NULL");
        Serial.printf("📍 gpsModule: %s\n", this->gpsModule ? "✅ OK" : "❌ NULL");
        Serial.printf("📻 fhfModule: %s\n", this->fhfModule ? "✅ OK" : "❌ NULL");
        Serial.printf("📻 commanderModule: %s\n", this->commanderModule? "✅ OK" : "❌ NULL");

        Serial.printf("📌 loraModule shared_ptr address: %p\n", this->loraModule.get());

        this->mainPage = lv_scr_act();
        this->infoBox = LVGLPage::createInfoBox();

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

            lv_timer_create([](lv_timer_t* t){
                delay(15000);
                auto *self = static_cast<CommandersMissionPage*>(t->user_data);
                sendShamir payload;
                payload.msgID = 0x03;
                payload.soldiersID = self->commanderModule->getCommanderNumber();

                char sharePath[25];
                snprintf(sharePath, sizeof(sharePath), "/share_%u.txt", payload.soldiersID);
                Serial.println(sharePath);

                File currentShamir = FFat.open(sharePath, FILE_READ);
                if (!currentShamir) 
                {
                    Serial.print("❌ Failed to open share file: ");
                    Serial.println(sharePath);
                    lv_timer_del(t);

                    return;
                }

                while (currentShamir.available()) 
                {
                    String line = currentShamir.readStringUntil('\n');
                    int comma = line.indexOf(',');
                    if (comma < 1) continue;

                    uint8_t x = (uint8_t)line.substring(0, comma).toInt();
                    uint8_t y = (uint8_t)line.substring(comma + 1).toInt();
                    payload.shamirPart.emplace_back(x, y);
                }

                currentShamir.close();

                FFatHelper::deleteFile(sharePath);

                std::string buffer;
                buffer.reserve(2 + payload.shamirPart.size() * 2);

                buffer += static_cast<char>(payload.msgID);
                buffer += static_cast<char>(payload.soldiersID);

                for(auto &pt : payload.shamirPart) 
                {
                    buffer.push_back((char)pt.first);
                    buffer.push_back((char)pt.second);
                }

                std::string base64Payload = crypto::CryptoModule::base64Encode(
                reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());
                    

                Serial.printf("ALL LENGTHS: %d %d %d\n", payload.shamirPart.size(), buffer.size(), base64Payload.length());
                self->onShamirPartReceived(reinterpret_cast<const uint8_t*>(base64Payload.c_str()), base64Payload.length());
                lv_timer_del(t);
            }, 100, this);

            
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
    lv_label_set_text(waitingLabel, "Waiting For Initial Coords");
    lv_obj_set_style_text_color(waitingLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    Serial.println("this->regularLoopTimer");
    this->regularLoopTimer = lv_timer_create([](lv_timer_t* t){
        auto *self = static_cast<CommandersMissionPage*>(t->user_data);
        lv_async_call([](void* user_data) 
        {
            auto *me = static_cast<CommandersMissionPage*>(user_data);

            me->loraModule->handleCompletedOperation();
            if (!me->loraModule->isBusy()) 
            {
                me->loraModule->readData();
            }


            me->loraModule->syncFrequency(me->fhfModule.get());
        }, self);
    }, 100, this);

    Serial.println("this->missingSoldierTimer");
    this->missingSoldierTimer = lv_timer_create([](lv_timer_t* t){
        auto *self = static_cast<CommandersMissionPage*>(t->user_data);
        if(self->commanderSwitchEvent)
        {
            return;
        }
        for (const auto& commander : self->commanderModule->getCommanders()) 
        {
            if(millis() - commander.second.lastTimeReceivedData >= 60000)
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
    int p1 = incoming.indexOf('|');
    int p2 = incoming.indexOf('|', p1 + 1);
    if (p1 < 0 || p2 < 0) {                      
        Serial.println("Bad ciphertext format");
        return;
    }
    Serial.println("Bad ciphertext format123");
    crypto::Ciphertext ct;
    ct.nonce = hexToBytes(incoming.substring(0, p1));
    ct.data = hexToBytes(incoming.substring(p1 + 1, p2));
    ct.tag = hexToBytes(incoming.substring(p2 + 1));

    crypto::ByteVec pt;

    try {
        pt = crypto::CryptoModule::decrypt(this->commanderModule->getGMK(), ct);
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

    String jsonStr;
    serializeJson(currentEvent, jsonStr);

    bool writeResult = FFatHelper::appendRegularJsonObjectToFile(this->logFilePath.c_str(), currentEvent);
    Serial.print("writeToFile result: ");
    Serial.println(writeResult ? "success" : "failure");

    Serial.println(this->logFilePath.c_str());

    std::tuple<int,int,int> tileLocation = positionToTile(tile_lat, tile_lon, 19);

    Serial.println("positionToTile");

    std::string middleTile = this->tileUrlInitial +
        std::to_string(std::get<0>(tileLocation)) + "/" +
        std::to_string(std::get<1>(tileLocation)) + "/" +
        std::to_string(std::get<2>(tileLocation)) + ".png";

    if(!FFatHelper::isFileExisting(this->tileFilePath))
    {
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
    }

    auto [zoom, x_tile, y_tile] = tileLocation;
    auto [centerLat, centerLon] = tileCenterLatLon(zoom, x_tile, y_tile);

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

    this->commanderModule->updateReceivedData(newG->soldiersID);
    const std::vector<uint8_t>& comp = this->commanderModule->getComp();

    if(newG->heartRate == 0 && std::find(comp.begin(), comp.end(), newG->soldiersID) == comp.end())
    {
        // Serial.println("CompromisedEvent");
        // this->commanderModule->setCompromised(newG->soldiersID);
        // delay(500);
        // const std::string newEventText = "Compromised Soldier Event - " + 
        // this->commanderModule->getCommanders().at(newG->soldiersID).name;
        // this->switchGMKEvent(newEventText.c_str(), newG->soldiersID);
        
        // JsonDocument doc;
        // doc["timestamp"] = CommandersMissionPage::getCurrentTimeStamp().c_str();
        // doc["eventName"] = "compromisedSoldier";
        // doc["compromisedID"] = newG->soldiersID;

        // FFatHelper::appendJSONEvent(this->logFilePath.c_str(), doc);

        this->switchCommanderEvent();
    }

}

inline crypto::ByteVec CommandersMissionPage::hexToBytes(const String& hex) 
{
    crypto::ByteVec out;
    out.reserve(hex.length() >> 1);
    for (int i = 0; i < hex.length(); i += 2) {
        out.push_back(strtoul(hex.substring(i, i + 2).c_str(), nullptr, 16));
    }
    return out;
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

    lv_obj_t* img1 = lv_img_create(lv_scr_act());
    lv_obj_center(img1);

    lv_img_set_src(img1, "A:/middleTile.png");
    lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);

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

std::tuple<int,int> CommandersMissionPage::latlon_to_pixel(double lat, double lon, double centerLat, double centerLon, int zoom)
{
    static constexpr double TILE_SIZE = 256.0;

    auto toGlobal = [&](double la, double lo) {
        double rad = la * M_PI / 180.0;
        double n   = std::pow(2.0, zoom);
        int xg = (int) std::floor((lo + 180.0) / 360.0 * n * TILE_SIZE);
        int yg = (int) std::floor((1.0 - std::log(std::tan(rad) + 1.0 / std::cos(rad)) / M_PI) / 2.0 * n * TILE_SIZE);
        return std::make_pair(xg, yg);
    };

    auto [markerX, markerY] = toGlobal(lat, lon);
    auto [centerX, centerY] = toGlobal(centerLat, centerLon);

    int localX = markerX - centerX + (LV_HOR_RES_MAX / 2);
    int localY = markerY - centerY + (LV_VER_RES_MAX / 2);

    return std::make_tuple(localX, localY);
}

void CommandersMissionPage::switchGMKEvent(const char* infoBoxText, uint8_t soldiersIDMoveToComp)
{
    Serial.println("switchGMKEvent");

    SwitchGMK payload;
    payload.msgID = 0x01;
    payload.soldiersID = soldiersIDMoveToComp;

    Serial.println("IMPORTANT INFO");
    std::string info("IMPORTANT INFO");
    crypto::ByteVec salt(16);
    randombytes_buf(salt.data(), salt.size());
    const crypto::Key256 newGMK = crypto::CryptoModule::deriveGK(this->commanderModule->getGMK(), millis(), info, salt, this->commanderModule->getCommanders().size());
    Serial.println("newGMK");
    for (const auto& soldier : this->commanderModule->getCommanders()) 
    {
        const crypto::Key256& keyRef = (soldier.first == soldiersIDMoveToComp ? this->commanderModule->getCompGMK() : newGMK);
        
        payload.encryptedGMK.clear();
        payload.soldiersID = soldier.first;
        Serial.printf("%d\n", payload.soldiersID);
        certModule::encryptWithPublicKey(this->commanderModule->getCommanders().at(payload.soldiersID).cert,
        crypto::CryptoModule::key256ToAsciiString(keyRef), payload.encryptedGMK);
        Serial.println("crypto::CryptoModule::key256ToAsciiString");
        std::string buffer;
        buffer += static_cast<char>(payload.msgID);
        buffer += static_cast<char>(payload.soldiersID);
        buffer.append(reinterpret_cast<const char*>(payload.encryptedGMK.data()), payload.encryptedGMK.size());

        Serial.printf("CERT: %s\n", certModule::certToString(this->commanderModule->getCommanders().at(soldier.first).cert).c_str());
        Serial.printf("GMK SENT: %s\n", crypto::CryptoModule::key256ToAsciiString(keyRef).c_str());

        std::string base64Payload = crypto::CryptoModule::base64Encode(
            reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());

        Serial.printf("PAYLOAD SENT (base64): %s %d\n", base64Payload.c_str(), base64Payload.length());

        Serial.println("SENDING base64Payload");
        this->loraModule->cancelReceive();
        this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(base64Payload.c_str()), base64Payload.length());

        delay(500);
    }
    
    this->commanderModule->setGMK(newGMK);
    LVGLPage::restartInfoBoxFadeout(this->infoBox, 1000, 5000, infoBoxText);
}

void CommandersMissionPage::missingSoldierEvent(uint8_t soldiersID, bool isCommander)
{
    Serial.printf("missingSoldierEvent for %d\n", soldiersID);

    std::string newEventText;

    if(isCommander)
    {
        newEventText = "Missing Soldier Event - " + this->commanderModule->getCommanders().at(soldiersID).name;
    }
    else
    {
        newEventText = "Missing Soldier Event - " + this->commanderModule->getSoldiers().at(soldiersID).name;
    }

    Serial.println("PRE PREMOVE");

    this->commanderModule->setMissing(soldiersID);

    this->switchGMKEvent(newEventText.c_str());

    JsonDocument doc;
    doc["timestamp"] = CommandersMissionPage::getCurrentTimeStamp().c_str();
    doc["eventName"] = "missingSoldier";
    doc["missingID"] = soldiersID;

    FFatHelper::appendJSONEvent(this->logFilePath.c_str(), doc);
}

void CommandersMissionPage::switchCommanderEvent()
{

    Serial.println("switchCommanderEvent");
    SwitchCommander payload;
    payload.msgID = 0x02;
    //Split here the log file, its path is this->logFilePath
    std::vector<String> sharePaths;

    lv_timer_del(this->missingSoldierTimer);

    
    this->loraModule->setOnReadData(nullptr);
    
    uint8_t nextID = -1;
    if(this->commanderModule->getCommandersInsertionOrder().size() > 1)
    {
        nextID = this->commanderModule->getCommandersInsertionOrder().at(1);
    }

    JsonDocument doc;
    doc["timestamp"] = CommandersMissionPage::getCurrentTimeStamp().c_str();
    doc["eventName"] = "commanderSwitch";
    doc["newCommanderID"] = nextID;

    FFatHelper::appendJSONEvent(this->logFilePath.c_str(), doc);

    uint8_t index = 0;

    std::vector<uint8_t> allSoldiers;
    allSoldiers.reserve(this->commanderModule->getCommanders().size() + this->commanderModule->getSoldiers().size());
    for (const auto& [k, v] : this->commanderModule->getCommanders()) 
    {
        if(!this->commanderModule->isComp(k))
        {
            allSoldiers.push_back(k);
        }
        
    }

    for (const auto& [k, v] : this->commanderModule->getSoldiers()) 
    {
        if(!this->commanderModule->isComp(k))
        {
            allSoldiers.push_back(k);
        }
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
        Serial.println(path);
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

        Serial.printf("Sending to soldier %d\n", soldier);
        payload.soldiersID = soldier;
        payload.shamirPart.clear();
        payload.compromisedSoldiers.clear();
        payload.missingSoldiers.clear();

        payload.compromisedSoldiers = this->commanderModule->getComp();
        payload.compromisedSoldiersLength = payload.compromisedSoldiers.size();

        payload.missingSoldiers = this->commanderModule->getMissing();
        payload.missingSoldiersLength = payload.missingSoldiers.size();

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
            uint8_t x = (uint8_t)line.substring(0, comma).toInt();
            uint8_t y = (uint8_t)line.substring(comma + 1).toInt();

            payload.shamirPart.emplace_back(x, y);
        }
        
        currentShamir.close();

        FFatHelper::deleteFile(sharePaths[index].c_str());

        uint16_t shamirPartsLen = payload.shamirPart.size();
        payload.shamirPartLength = shamirPartsLen;

        std::string buffer;
        buffer += static_cast<char>(payload.msgID);
        buffer += static_cast<char>(payload.soldiersID);
        buffer += static_cast<char>((shamirPartsLen >> 8) & 0xFF);
        buffer += static_cast<char>(shamirPartsLen & 0xFF);
        
        for(auto &pt : payload.shamirPart) 
        {
            buffer.push_back((char)pt.first);
            buffer.push_back((char)pt.second);
        }
        
        buffer += static_cast<char>(payload.compromisedSoldiersLength);
        buffer.append(reinterpret_cast<const char*>(payload.compromisedSoldiers.data()), payload.compromisedSoldiers.size());
        buffer += static_cast<char>(payload.missingSoldiersLength);
        buffer.append(reinterpret_cast<const char*>(payload.missingSoldiers.data()), payload.missingSoldiers.size());

        std::string base64Payload = crypto::CryptoModule::base64Encode(
            reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());

        Serial.printf("PAYLOAD SENT (base64): %d %d %d\n", shamirPartsLen, payload.compromisedSoldiersLength, payload.missingSoldiersLength);
        Serial.println(base64Payload.c_str());
        Serial.println("SENDING base64Payload");
        this->loraModule->cancelReceive();
        this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(base64Payload.c_str()), base64Payload.length());
    
        ++index;

        delay(2000);
    }

    Serial.println("finished loop");

    lv_timer_del(this->regularLoopTimer);

    Serial.println("deleted timer");

    this->commanderModule->removeFirstCommanderFromInsertionOrder();

    std::unique_ptr<Soldier> sold = std::make_unique<Soldier>(this->commanderModule->getName(),
     this->commanderModule->getPublicCert(), this->commanderModule->getPrivateKey(),
     this->commanderModule->getCAPublicCert(), this->commanderModule->getCommanderNumber(),
     this->commanderModule->getIntervalMS());

    sold->setInsertionOrders(this->commanderModule->getCommandersInsertionOrder());
    sold->setCommanders(this->commanderModule->getCommanders());

    Serial.println("created sold");
    this->destroyPage();
    delay(10);

    Serial.println("destroyPage");

    this->commanderModule->clear();
    this->ballColors.clear();
    this->markers.clear();
    this->labels.clear();

    Serial.println("this->transferFunction");

    this->transferFunction(this->loraModule, std::move(this->wifiModule),
     this->gpsModule, std::move(this->fhfModule), std::move(sold));
}


void CommandersMissionPage::onShamirPartReceived(const uint8_t* data, size_t len)
{
    Serial.println("onShamirPartReceived");
    if (len < 4) {
        Serial.println("Received data too short for any struct with length prefix");
        return;
    }

    std::string base64Str(reinterpret_cast<const char*>(data), len);
    Serial.printf("DECODED DATA: %s %d\n", base64Str.c_str(), len);

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

    sendShamir payload;

    payload.msgID = static_cast<uint8_t>(decodedData[0]);
    payload.soldiersID = static_cast<uint8_t>(decodedData[1]);

    if(payload.msgID != 0x03)
    {
        Serial.printf("ID is not 0x03! %d\n", payload.msgID);
        return;
    }

    uint16_t shamirPartsLength = (decodedData.size() - 2) / 2;
    payload.shamirPart.reserve(shamirPartsLength);

    size_t idx = 2;
    for(int k = 0; k < shamirPartsLength; ++k) 
    {
        uint8_t x = uint8_t(decodedData[idx++]);
        uint8_t y = uint8_t(decodedData[idx++]);
        payload.shamirPart.emplace_back(x,y);
    }

    Serial.printf("Deserialized SwitchGMK: msgID=%d, soldiersID=%d, shamirPart size=%zu\n",
                  payload.msgID, payload.soldiersID, payload.shamirPart.size());

    if(payload.shamirPart.size() == 0)
    {
        return;
    }

    char sharePath[25];
    snprintf(sharePath, sizeof(sharePath), "/share_%u.txt", payload.soldiersID);
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

        uint16_t bytesWritten = 0;
        for (auto &pt : payload.shamirPart) 
        {
            currentShare.printf("%u,%u\n", pt.first, pt.second);
            bytesWritten++;
        }

        if (bytesWritten != payload.shamirPart.size()) 
        {
            Serial.printf("⚠️  Only wrote %u/%u bytes to \"%s\"\n",
                        (unsigned)bytesWritten, (unsigned)payload.shamirPart.size(), sharePath);
        } 
        else 
        {
            Serial.printf("✅ Wrote %u bytes to \"%s\"\n",
                        (unsigned)bytesWritten, sharePath);
            
            this->shamirPartsCollected++;
            this->didntSendShamir.erase(this->didntSendShamir.begin());
        }

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
        
        this->commanderSwitchEvent = false;

        this->commanderModule->resetAllData();

        this->loraModule->setOnReadData([this](const uint8_t* data, size_t len) {
                this->onDataReceived(data, len);
        });

        this->loraModule->setOnFileReceived(nullptr);

        this->didntSendShamir.clear();

        return;
    } 

    if (this->shamirPartsCollected == 1 && !this->loraModule->getOnFileReceived())
    {
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

    requestShamir req;
    req.msgID = 0x04;
    req.soldiersID = this->currentShamirRec;

    std::string buffer;
    buffer += static_cast<char>(req.msgID);
    buffer += static_cast<char>(req.soldiersID);

    std::string base64Payload = crypto::CryptoModule::base64Encode(
    reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());
        
    Serial.printf("Sending requestShamir to %d\n", req.soldiersID);

    this->loraModule->cancelReceive();
    this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(base64Payload.c_str()), base64Payload.length());


    shamirTimeoutTimer = lv_timer_create(
        [](lv_timer_t* t) {
            auto *self = static_cast<CommandersMissionPage*>(t->user_data);
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
                self->sendNextShamirRequest();
            }

            Serial.println("shamirTimeoutTimer finished!");
        },
        20000,
        this
    );
}