#include "CommandersMissionPage.h"

CommandersMissionPage::CommandersMissionPage(std::shared_ptr<LoraModule> loraModule, std::unique_ptr<WifiModule> wifiModule,
     std::shared_ptr<GPSModule> gpsModule, std::unique_ptr<Commander> commanderModule, const std::string& logFilePath, bool fakeGPS)
    : logFilePath(logFilePath) 
    {
        this->loraModule = std::move(loraModule);
        this->wifiModule = std::move(wifiModule);
        this->gpsModule = std::move(gpsModule);
        this->commanderModule = std::move(commanderModule);

        this->mainPage = lv_scr_act();
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
    } catch (const std::exception& e) {
        Serial.printf("Decryption failed: %s\n", e.what());
        return;
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

    if(isZero(tile_lat) || isZero(tile_lon) || isZero(marker_lat) || isZero(marker_lon))
    {
        return;
    }

    struct tm timeInfo;
    char timeStr[9];
    if (getLocalTime(&timeInfo))
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeInfo);
    else
        strcpy(timeStr, "00:00:00");

    char result[128];
    snprintf(result, sizeof(result), "%s - Soldier 1: {%.5f, %.5f}", 
             timeStr, tile_lat, tile_lon);

    FFatHelper::writeToFile(this->logFilePath.c_str(), result);

    std::tuple<int,int,int> tileLocation = positionToTile(tile_lat, tile_lon, 19);

    std::string middleTile = this->tileUrlInitial +
        std::to_string(std::get<0>(tileLocation)) + "/" +
        std::to_string(std::get<1>(tileLocation)) + "/" +
        std::to_string(std::get<2>(tileLocation)) + ".png";

    if(!FFatHelper::isFileExisting(this->tileFilePath))
    {
        this->wifiModule->downloadFile(middleTile.c_str(), this->tileFilePath);
    }

    auto [zoom, x_tile, y_tile] = tileLocation;
    auto [centerLat, centerLon] = tileCenterLatLon(zoom, x_tile, y_tile);

    int heartRate = newG->heartRate;
    if(newG->soldiersID == 1)
    {
        ballColor = getColorFromHeartRate(heartRate);
        create_fading_circle(marker_lat, marker_lon, centerLat, centerLon, newG->soldiersID, 19, &ballColor, current_marker, soldiersNameLabel);
    }
    else
    {
        secondBallColor = getColorFromHeartRate(heartRate);
        create_fading_circle(marker_lat, marker_lon, centerLat, centerLon, newG->soldiersID, 19, &secondBallColor, current_second_marker, secondSoldiersNameLabel);
    }
    
    if(newG->heartRate == 0)
    {
        Serial.println("compromisedEvent");
        compromisedEvent();
        init_main_menu();
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
    lv_obj_t* img1 = lv_img_create(this->mainPage);
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