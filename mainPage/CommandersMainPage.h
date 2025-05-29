#pragma once

#include <LVGLPage.h>
#include <LilyGoLib.h>
#include <LoraModule.h>
#include <WifiModule.h>
#include <GPSModule.h>

#include "UploadLogPage.h"
#include "../missionPage/CommandersMissionPage.h"
#include "ReceiveLogsPage.h"

class CommandersMainPage : public LVGLPage {
    private:
    lv_obj_t* mainPage;
    std::function<void(std::unique_ptr<WifiModule>)> onTransferReceiveCoordsPage;
    std::function<void(std::unique_ptr<WifiModule>)> onTransferUploadLogsPage;
    std::unique_ptr<WifiModule> wifiModule;

    public:
    CommandersMainPage(std::unique_ptr<WifiModule>);
    void createPage();

    void setOnTransferReceiveCoordsPage(std::function<void(std::unique_ptr<WifiModule>)> cb);
    void setOnTransferUploadLogsPage(std::function<void(std::unique_ptr<WifiModule>)> cb);
};