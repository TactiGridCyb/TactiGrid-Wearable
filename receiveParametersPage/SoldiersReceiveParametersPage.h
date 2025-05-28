#pragma once

#include <LVGLPage.h>
#include <LV_Helper.h>
#include <WifiModule.h>
#include <ArduinoJson.h>
#include <Soldier.h>
#include <certModule.h>

class SoldiersReceiveParametersPage : public LVGLPage{
    public:
        SoldiersReceiveParametersPage(std::unique_ptr<WifiModule> wifiModule);

        void createPage() override;
        void destroy();

    private:
        std::unique_ptr<WifiModule> wifiModule;
        lv_obj_t* mainPage;
        lv_obj_t* statusLabels[6];

        std::unique_ptr<Soldier> soldierModule;

        const char* messages[6] = {
            "Received Cert", "Received CA Cert", "Received GMK", "Received Freqs", "Received Interval", "Received Commanders Certs"
        };

        void onSocketOpened(lv_event_t* event);

        void updateLabel(uint8_t index);
};