#pragma once

#include <LVGLPage.h>
#include <LV_Helper.h>
#include <WifiModule.h>
#include <ArduinoJson.h>
#include <Soldier.h>
#include <certModule.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pk.h>
#include "mbedtls/oid.h"
#include <stdexcept>
#include <string>
#include "../env.h"
#include "../mainPage/SoldiersMainPage.h"

class SoldiersReceiveParametersPage : public LVGLPage {
    public:
        SoldiersReceiveParametersPage(std::unique_ptr<WifiModule> wifiModule);

        void createPage() override;

        void setOnTransferPage(std::function<void(std::unique_ptr<WifiModule>, std::unique_ptr<Soldier>)> cb);

        static std::string extractPemBlock(const std::string& blob,
                                   const char* header,
                                   const char* footer);

    private:
        std::unique_ptr<WifiModule> wifiModule;
        lv_obj_t* mainPage;
        lv_obj_t* openSocketButton;
        lv_obj_t* statusLabels[6];

        std::unique_ptr<Soldier> soldierModule;

        std::function<void(std::unique_ptr<WifiModule>, std::unique_ptr<Soldier>)> onTransferPage;

        const char* messages[6] = {
            "Received Cert", "Received CA Cert", "Received GMK", "Received Freqs", "Received Interval", "Received Commanders Certs"
        };

        void onSocketOpened(lv_event_t* event);

        void updateLabel(uint8_t index);
};