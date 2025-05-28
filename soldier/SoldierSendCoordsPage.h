#pragma once

#include <cstring>
#include <LVGLPage.h>
#include <LilyGoLib.h>
#include <LoraModule.h>
#include <WifiModule.h>
#include <GPSModule.h>
#include <SoldiersSentData.h>
#include <CryptoModule.h>

#include <LVGLPage.h>

class SoldierSendCoordsPage : public LVGLPage
{
    private:
    std::shared_ptr<LoraModule> loraModule;
    std::unique_ptr<WifiModule> wifiModule;
    std::shared_ptr<GPSModule> gpsModule;

    lv_obj_t* mainPage;
    lv_obj_t* sendLabel;

    uint16_t currentIndex;
    uint16_t coordCount;

    crypto::Key256 sharedKey;

    bool fakeGPS;

    static void sendTimerCallback(lv_timer_t *);

    void sendCoordinate(float, float, uint16_t, uint16_t);

    public:
    SoldierSendCoordsPage(std::shared_ptr<LoraModule>, std::unique_ptr<WifiModule>, std::shared_ptr<GPSModule>, bool = true);
    void createPage();

    static std::pair<float, float> getTileCenterLatLon(float lat, float lon, int zoomLevel, float tileSize);

    void parseGPSData();
};