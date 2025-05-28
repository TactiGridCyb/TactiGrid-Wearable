#pragma once

#include <LVGLPage.h>
#include <LilyGoLib.h>
#include <WifiModule.h>

#include <SoldierSendCoordsPage.h>

class SoldiersMainPage : public LVGLPage {
    private:
    std::unique_ptr<WifiModule> wifiModule;
    lv_obj_t* mainPage;

    const char* helmentDownloadLink = "https://iconduck.com/api/v2/vectors/vctr0xb8urkk/media/png/256/download";

    public:
    SoldiersMainPage(std::unique_ptr<WifiModule>);
    void createPage();

    void showHelmet();

    void transferToSendCoordsPage(lv_event_t*);
};