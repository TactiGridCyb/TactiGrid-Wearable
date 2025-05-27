#pragma once

#include <LVGLPage.h>
#include <LilyGoLib.h>
#include <LoraModule.h>
#include <WifiModule.h>
#include <GPSModule.h>

#include <SoldierSendCoordsPage.h>
#include "UploadLogPage.h"
#include "MainPocPage.h"
#include "ReceiveLogsPage.h"

class CommandersMainPage : public LVGLPage {
    private:
    std::shared_ptr<LoraModule> loraModule;
    std::shared_ptr<WifiModule> wifiModule;
    std::shared_ptr<GPSModule> gpsModule;

    std::shared_ptr<UploadLogPage> uploadLogPage;
    std::shared_ptr<MainPocPage> mainPocPage;
    std::shared_ptr<ReceiveLogsPage> receiveLogsPage;

    lv_obj_t* mainPage;

    public:
    CommandersMainPage(std::unique_ptr<LoraModule>, std::unique_ptr<WifiModule>, std::unique_ptr<GPSModule>);
    void createPage();
    void showPage();
    void destroyPage();
};