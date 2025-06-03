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
#include "../commander/Soldier.h"
#include <FHFModule.h>

class CommandersMissionPage : public LVGLPage {
public:
    CommandersMissionPage(std::shared_ptr<LoraModule>, std::unique_ptr<WifiModule>,
         std::shared_ptr<GPSModule>, std::unique_ptr<FHFModule>, std::unique_ptr<Commander>, const std::string&, bool = true);

    void createPage() override;

    static inline crypto::ByteVec hexToBytes(const String& hex);

    static inline bool isZero(float);

    static std::tuple<int, int, int> positionToTile(float lat, float lon, int zoom);

    const std::string tileUrlInitial = "https://tile.openstreetmap.org/";
    const char* tileFilePath = "/middleTile.png";

    static std::pair<double,double> tileCenterLatLon(int zoom, int x_tile, int y_tile);

    void create_fading_circle(double markerLat, double markerLon, double centerLat, double centerLon, uint16_t soldiersID, int zoom,
     lv_color_t* ballColor, lv_obj_t*& marker, lv_obj_t*& label);
    
    static std::tuple<int,int> latlon_to_pixel(double lat, double lon, double centerLat, double centerLon, int zoom);

    void setTransferFunction(std::function<void(std::shared_ptr<LoraModule>, std::unique_ptr<WifiModule>,
         std::shared_ptr<GPSModule>, std::unique_ptr<FHFModule>, std::unique_ptr<Soldier>)> cb);

private:
    std::shared_ptr<LoraModule> loraModule;
    std::unique_ptr<WifiModule> wifiModule;
    std::shared_ptr<GPSModule> gpsModule;
    std::unique_ptr<FHFModule> fhfModule;
    std::unique_ptr<Commander> commanderModule;

    std::string logFilePath;

    bool fakeGPS;
    
    lv_obj_t* infoBox;

    lv_obj_t* mainPage;

    std::unordered_map<uint16_t, lv_color_t> ballColors;
    std::unordered_map<uint16_t, lv_obj_t*> labels;
    std::unordered_map<uint16_t, lv_obj_t*> markers;

    std::function<void(std::shared_ptr<LoraModule>, std::unique_ptr<WifiModule>,
         std::shared_ptr<GPSModule>, std::unique_ptr<FHFModule>, std::unique_ptr<Soldier>)> transferFunction;

    void onDataReceived(const uint8_t* data, size_t len);
    void showMiddleTile();

    void switchGMKEvent(const char* infoBoxText, uint8_t soldiersIDMoveToComp = -1);
    void missingSoldierEvent(uint8_t soldiersID);
    void switchCommanderEvent(const char* infoBoxText);
    

};