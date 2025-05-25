#pragma once

#include <LVGLPage.h>
#include <LilyGoLib.h>
#include <LoraModule.h>
#include <WifiModule.h>
#include <GPSModule.h>

#include <SoldierSendCoordsPage.h>

class SoldiersMainPage : public LVGLPage {
    private:
    std::unique_ptr<LoraModule> loraModule;
    std::unique_ptr<WifiModule> wifiModule;
    std::unique_ptr<GPSModule> gpsModule;

    lv_obj_t* mainPage;


    public:
    SoldiersMainPage(std::unique_ptr<LoraModule>, std::unique_ptr<WifiModule>, std::unique_ptr<GPSModule>);
    void createPage();
    void showPage();
    void destroyPage();

    void showHelmet();

    void transferToSendCoordsPage(lv_event_t*);
};