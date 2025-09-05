// DiffieHellmanPageCommander.h
#pragma once
#include <LVGLPage.h>
#include <functional>
#include "../certModule/certModule.h"
#include "../LoraModule/LoraModule.h"
#include <map>
#include "../commander-DH/CommanderECDHHandler.h"
#include <LilyGoLib.h>
#include <LoraModule.h>
#include <WifiModule.h>
#include <GPSModule.h>

class DiffieHellmanPageCommander : public LVGLPage {
private:
    lv_obj_t* page;
    lv_obj_t* statusLabel;
    lv_obj_t* startBtn;
    Commander* commander;
    CommanderECDHHandler* dhHandler = nullptr;
    certModule certmodule;
    std::function<void()> onStart;
    bool commanderProcessStarted;
    bool exchangeInProgress;
    std::vector<uint8_t> allSoldiersVector;
    int currentSoldierIndex = 0;
    std::unique_ptr<WifiModule> wifiModule;
    std::unique_ptr<Commander> commanderPtr;

    std::function<void(std::unique_ptr<WifiModule>, std::unique_ptr<Commander>)> onTransferMainPage;

public:
    DiffieHellmanPageCommander(std::unique_ptr<WifiModule> wifiModule, std::unique_ptr<Commander> commanderPtr, Commander* commander);
    void createPage() override;
    void setStatusText(const char* text);
    void setOnStartCallback(std::function<void()> cb);

    void startExchangeWithNextSoldier();

    // Internal methods
    void startProcess();
    void poll();

    // LVGL button event handler
    static void startBtnEvent(lv_event_t* e) {
        auto* self = static_cast<DiffieHellmanPageCommander*>(lv_event_get_user_data(e));
        self->onButtonPressed();
    }

    void setOnTransferMainPage(std::function<void(std::unique_ptr<WifiModule>, std::unique_ptr<Commander>)> cb);

protected:
    // after pressing the button check if the process is ok to start
    virtual void onButtonPressed() {
        if (!commanderProcessStarted) {
            startProcess();
        }
    }
};