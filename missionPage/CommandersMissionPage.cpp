#include "CommandersMissionPage.h"

CommandersMissionPage::CommandersMissionPage(std::shared_ptr<LoraModule> loraModule, std::unique_ptr<WifiModule> wifiModule,
     std::shared_ptr<GPSModule> gpsModule, std::unique_ptr<FHFModule> fhfModule, std::unique_ptr<Commander> commanderModule, const std::string& logFilePath, bool fakeGPS)
    : logFilePath(logFilePath) 
    {
        this->loraModule = std::move(loraModule);
        this->wifiModule = std::move(wifiModule);
        this->gpsModule = std::move(gpsModule);
        this->fhfModule = std::move(fhfModule);
        this->commanderModule = std::move(commanderModule);

        Serial.println("🔍 Checking modules:");
        Serial.printf("📡 loraModule: %s\n", this->loraModule ? "✅ OK" : "❌ NULL");
        Serial.printf("🌐 wifiModule: %s\n", this->wifiModule ? "✅ OK" : "❌ NULL");
        Serial.printf("📍 gpsModule: %s\n", this->gpsModule ? "✅ OK" : "❌ NULL");
        Serial.printf("📻 fhfModule: %s\n", this->fhfModule ? "✅ OK" : "❌ NULL");
        Serial.printf("📻 commanderModule: %s\n", this->commanderModule? "✅ OK" : "❌ NULL");

        this->mainPage = lv_scr_act();

        this->loraModule->setOnReadData([this](const uint8_t* data, size_t len) {
            this->onDataReceived(data, len);
        });

        FFatHelper::deleteFile(this->logFilePath.c_str());
        FFatHelper::deleteFile(this->tileFilePath);

        this->infoBox = LVGLPage::createInfoBox();
    }

void CommandersMissionPage::createPage() {
    FFatHelper::deleteFile(this->logFilePath.c_str());


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


    this->regularLoopTimer = lv_timer_create([](lv_timer_t* t){
        auto *self = static_cast<CommandersMissionPage*>(t->user_data);
        self->loraModule->handleCompletedOperation();

        if (!self->loraModule->isBusy()) {
            self->loraModule->readData();
        }

        self->loraModule->syncFrequency(self->fhfModule.get());
    }, 100, this);

    this->missingSoldierTimer = lv_timer_create([](lv_timer_t* t){
        auto *self = static_cast<CommandersMissionPage*>(t->user_data);
        for (const auto& soldier : self->commanderModule->getCommanders()) 
        {
            if(millis() - soldier.second.lastTimeReceivedData >= 60000)
            {
                self->missingSoldierEvent(soldier.first);
                break;
            }
        }
    }, 60000, this);
}

void CommandersMissionPage::onDataReceived(const uint8_t* data, size_t len)
{
    String incoming;
    for (size_t i = 0; i < len; i++) 
    {
        incoming += (char)data[i];
    }

    int p1 = incoming.indexOf('|');
    int p2 = incoming.indexOf('|', p1 + 1);
    if (p1 < 0 || p2 < 0) {                      
        Serial.println("Bad ciphertext format");
        return;
    }

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
    struct tm timeInfo;
    char timeStr[25];
    if (getLocalTime(&timeInfo, 3000))
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%dT%H:%M:%SZ", &timeInfo);
    else
        strcpy(timeStr, "1970-01-01T00:00:00Z");

    Serial.println("getLocalTime");

    JsonDocument currentEvent;
    currentEvent["timestamp"] = timeStr;
    currentEvent["isEvent"] = false;

    JsonObject soldInfo = currentEvent.createNestedObject("data");
    soldInfo["lat"] = marker_lat;
    soldInfo["lon"] = marker_lon;
    soldInfo["heartRate"] = newG->heartRate;
    soldInfo["name"] = this->commanderModule->getCommanders().at(newG->soldiersID).name;

    String jsonStr;
    serializeJson(currentEvent, jsonStr);

    bool writeResult = FFatHelper::appendJsonObjectToFile(this->logFilePath.c_str(), jsonStr.c_str());
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

    if(newG->heartRate == 0 && !this->commanderModule->getCommanders().at(newG->soldiersID).isComp)
    {
        Serial.println("CompromisedEvent");
        // this->commanderModule->setCompromised(newG->soldiersID);
        // delay(500);
        // const std::string newEventText = "Compromised Soldier Event - " + 
        // this->commanderModule->getCommanders().at(newG->soldiersID).name;
        // this->switchGMKEvent(newEventText.c_str(), newG->soldiersID);

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

        const auto& commanders = this->commanderModule->getCommanders();
        for (const auto& kv : commanders) {
            Serial.printf("Other ID: %u\n", kv.first);
        }
        auto it = commanders.find(soldiersID);

        if (it != commanders.end()) {
            Serial.printf("Soldier %u found in map!!\n", soldiersID);
            std::string s = it->second.name;
            lv_label_set_text(label, s.c_str());
        } else {
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
    }
    
    this->commanderModule->setGMK(newGMK);
    LVGLPage::restartInfoBoxFadeout(this->infoBox, 1000, 5000, infoBoxText);
}

void CommandersMissionPage::missingSoldierEvent(uint8_t soldiersID)
{
    Serial.printf("missingSoldierEvent for %d\n", soldiersID);
    const std::string newEventText = "Missing Soldier Event - " + this->commanderModule->getCommanders().at(soldiersID).name;

    Serial.println("PRE PREMOVE");
    this->commanderModule->removeOther(soldiersID);

    this->switchGMKEvent(newEventText.c_str());
}

void CommandersMissionPage::switchCommanderEvent()
{
    Serial.println("switchCommanderEvent");
    SwitchCommander payload;
    payload.msgID = 0x02;
    //Split here the log file, its path is this->logFilePath
    std::vector<String> sharePaths;

    lv_timer_del(this->missingSoldierTimer);

    if (!ShamirHelper::splitFile(this->logFilePath.c_str(), this->commanderModule->getCommanders().size(), sharePaths)) 
    {
        Serial.println("❌ Failed to split log file with Shamir.");
        return;
    }
    this->loraModule->setOnReadData(nullptr);
    uint8_t index = 0;

    for (const auto& soldier : this->commanderModule->getCommanders()) 
    {
        Serial.println("Sending to soldier");
        payload.soldiersID = soldier.first;
        payload.shamirPart.clear();

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
            if (comma < 0) continue;
            int y = line.substring(comma + 1).toInt();
            payload.shamirPart.push_back((uint8_t)y);
        }
        currentShamir.close();


        std::string buffer;
        buffer += static_cast<char>(payload.msgID);
        buffer += static_cast<char>(payload.soldiersID);
        buffer.append(reinterpret_cast<const char*>(payload.shamirPart.data()), payload.shamirPart.size());

        std::string base64Payload = crypto::CryptoModule::base64Encode(
            reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size());

        Serial.printf("PAYLOAD SENT (base64): %s %d %d\n", base64Payload.c_str(), base64Payload.length(), payload.shamirPart.size());

        Serial.println("SENDING base64Payload");
        this->loraModule->cancelReceive();
        this->loraModule->sendFile(reinterpret_cast<const uint8_t*>(base64Payload.c_str()), base64Payload.length());
    
        ++index;

        delay(500);
    }

    Serial.println("finished loop");

    lv_timer_del(this->regularLoopTimer);

    Serial.println("deleted timer");
    
    std::unique_ptr<Soldier> sold = std::make_unique<Soldier>(this->commanderModule->getName(),
     this->commanderModule->getPublicCert(), this->commanderModule->getPrivateKey(),
     this->commanderModule->getCAPublicCert(), this->commanderModule->getCommanderNumber(),
     this->commanderModule->getIntervalMS());
    

    Serial.println("created sold");
    this->destroyPage();
    delay(10);

    Serial.println("destroyPage");

    this->transferFunction(this->loraModule, std::move(this->wifiModule),
     this->gpsModule, std::move(this->fhfModule), std::move(sold));

    Serial.println("this->transferFunction");

    this->commanderModule->clear();
    this->ballColors.clear();
    this->markers.clear();
    this->labels.clear();
}
