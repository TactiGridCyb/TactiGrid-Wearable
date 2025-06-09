#pragma once

#include <LVGLPage.h>
#include <LilyGoLib.h>
#include <LoraModule.h>
#include <WifiModule.h>
#include <GPSModule.h>

#include "../missionPage/CommandersMissionPage.h"
#include "ReceiveLogsPage.h"

class CommandersMainPage : public LVGLPage {
    private:
    lv_obj_t* mainPage;
    std::function<void()> onTransferReceiveCoordsPage;

    public:
    CommandersMainPage();
    void createPage();

    void setOnTransferReceiveCoordsPage(std::function<void()> cb);
};