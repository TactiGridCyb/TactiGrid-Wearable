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
#include <FHFModule.h>
#include <Commander.h>
#include <ShamirHelper.h>
#include <LVGLPage.h>

class SoldiersMissionPage : public LVGLPage
{
    private:
    std::shared_ptr<LoraModule> loraModule;
    std::shared_ptr<GPSModule> gpsModule;
    std::unique_ptr<FHFModule> fhfModule;
    std::unique_ptr<Soldier> soldierModule;

    lv_obj_t* mainPage;
    lv_obj_t* sendLabel;

    uint16_t currentIndex;
    uint16_t coordCount;

    lv_timer_t* sendTimer;
    lv_timer_t* mainLoopTimer;

    std::function<void(std::shared_ptr<LoraModule>, std::shared_ptr<GPSModule>, 
        std::unique_ptr<FHFModule>, std::unique_ptr<Commander>)> transferFunction;

    bool fakeGPS;
    bool delaySendFakeGPS;

    static constexpr SoldiersSentData coords[5] = {
        {0.0f, 0.0f, 31.970866f, 34.785664f,  78, 2},
        {0.0f, 0.0f, 31.970870f, 34.785683f, 100, 3},
        {0.0f, 0.0f, 31.970855f, 34.785643f,  55, 2}, 
        {0.0f, 0.0f, 31.970840f, 34.785623f,   0, 2},
        {0.0f, 0.0f, 31.970880f, 34.785703f, 120, 3}
    };

    static void sendTimerCallback(lv_timer_t *);

    void sendCoordinate(float, float, uint16_t, uint16_t);

    void onGMKSwitchEvent(SwitchGMK);

    void onDataReceived(const uint8_t*, size_t);

    void onCommanderSwitchEvent(SwitchCommander&);

    public:
    SoldiersMissionPage(std::shared_ptr<LoraModule>,
         std::shared_ptr<GPSModule>, std::unique_ptr<FHFModule>, std::unique_ptr<Soldier>, bool = true);
    void createPage();

    static std::pair<float, float> getTileCenterLatLon(float lat, float lon, int zoomLevel, float tileSize);

    void parseGPSData();

    void setTransferFunction(std::function<void(std::shared_ptr<LoraModule>, std::shared_ptr<GPSModule>, 
        std::unique_ptr<FHFModule>, std::unique_ptr<Commander>)> cb);

    static inline bool isZero(float);
};