#pragma once

#include <LVGLPage.h>
#include <LilyGoLib.h>
#include <WifiModule.h>

#include "../missionPage/SoldiersMissionPage.h"

class SoldiersMainPage : public LVGLPage {
    private:
    std::unique_ptr<WifiModule> wifiModule;
    lv_obj_t* mainPage;

    const char* helmentDownloadLink = "https://iconduck.com/api/v2/vectors/vctr0xb8urkk/media/png/256/download";

    std::function<void(std::unique_ptr<WifiModule>)> onTransferPage;

    public:
    SoldiersMainPage(std::unique_ptr<WifiModule>);
    void createPage();

    void showHelmet();

    void setOnTransferPage(std::function<void(std::unique_ptr<WifiModule>)> cb);
};