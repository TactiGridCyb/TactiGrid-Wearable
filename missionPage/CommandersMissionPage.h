#pragma once

#include <LVGLPage.h>
#include <LoraModule.h>
#include <WifiModule.h>
#include <GPSModule.h>
#include <string>
#include <memory>
#include <CryptoModule.h>
#include <FFatHelper.h>
#include <Commander.h>
#include <SoldiersSentData.h>

class CommandersMissionPage : public LVGLPage {
public:
    CommandersMissionPage(std::shared_ptr<LoraModule>, std::unique_ptr<WifiModule>,
         std::shared_ptr<GPSModule>, std::unique_ptr<Commander>, const std::string&, bool = true);

    void createPage() override;

    static inline crypto::ByteVec hexToBytes(const String& hex);

    static inline bool isZero(float);

    static std::tuple<int, int, int> positionToTile(float lat, float lon, int zoom);

    const std::string tileUrlInitial = "https://tile.openstreetmap.org/";
    const char* tileFilePath = "/middleTile.png";

    static std::pair<double,double> CommandersMissionPage::tileCenterLatLon(int zoom, int x_tile, int y_tile);

private:
    std::shared_ptr<LoraModule> loraModule;
    std::unique_ptr<WifiModule> wifiModule;
    std::shared_ptr<GPSModule> gpsModule;
    std::unique_ptr<Commander> commanderModule;

    std::string logFilePath;

    bool fakeGPS;


    lv_obj_t* mainPage;

    std::unordered_map<uint16_t, lv_color_t> ballColors;
    std::unordered_map<uint16_t, lv_obj_t*> labels;
    std::unordered_map<uint16_t, lv_color_t> markers;

    void onDataReceived(const uint8_t* data, size_t len);
    void showMiddleTile();
};