#pragma once

#include <cstring>
#include <LVGLPage.h>
#include <LilyGoLib.h>
#include <LoraModule.h>
#include <WifiModule.h>
#include <GPSModule.h>
#include <SoldiersSentData.h>
#include <CryptoModule.h>
#include <Soldier.h>

#include <LVGLPage.h>

class SoldiersMissionPage : public LVGLPage
{
    private:
    std::shared_ptr<LoraModule> loraModule;
    std::unique_ptr<WifiModule> wifiModule;
    std::shared_ptr<GPSModule> gpsModule;
    std::unique_ptr<Soldier> soldierModule;

    lv_obj_t* mainPage;
    lv_obj_t* sendLabel;

    uint16_t currentIndex;
    uint16_t coordCount;

    bool fakeGPS;

    static constexpr SoldiersSentData coords[5] = {
        {0.0f, 0.0f, 31.970866f, 34.785664f,  78, 2, REGULAR},
        {0.0f, 0.0f, 31.970870f, 34.785683f, 100, 3, REGULAR},
        {0.0f, 0.0f, 31.970855f, 34.785643f,  55, 2, REGULAR}, 
        {0.0f, 0.0f, 31.970840f, 34.785623f,   0, 3, REGULAR},
        {0.0f, 0.0f, 31.970880f, 34.785703f, 120, 2, REGULAR}
    };

    static void sendTimerCallback(lv_timer_t *);

    void sendCoordinate(float, float, uint16_t, uint16_t);

    public:
    SoldiersMissionPage(std::shared_ptr<LoraModule>, std::unique_ptr<WifiModule>,
         std::shared_ptr<GPSModule>, std::unique_ptr<Soldier>, bool = true);
    void createPage();

    static std::pair<float, float> getTileCenterLatLon(float lat, float lon, int zoomLevel, float tileSize);

    void parseGPSData();

    static inline bool isZero(float);
};