// DiffieHellmanPage.h
#pragma once
#include <LVGLPage.h>
#include <functional>
#include "../certModule/certModule.h"
#include "../LoraModule/LoraModule.h"
#include "../soldier-DH/SoldierECDHHandler.h"
#include <LilyGoLib.h>
#include <LoraModule.h>
#include <WifiModule.h>
#include <GPSModule.h>

class DiffieHellmanPageSoldier : public LVGLPage {
private:
    lv_obj_t* page;
    lv_obj_t* statusLabel;
    lv_obj_t* startBtn;
    Soldier* soldier;
    SoldierECDHHandler* dhHandler = nullptr;
    certModule certmodule;
    std::function<void()> onStart;

    bool soldierProcessStarted;

    std::unique_ptr<WifiModule> wifiModule;
    std::unique_ptr<Soldier> soldierPtr;

    std::function<void(std::unique_ptr<WifiModule>, std::unique_ptr<Soldier>)> onTransferMainPage;

public:
    DiffieHellmanPageSoldier(std::unique_ptr<WifiModule> wifiModule, std::unique_ptr<Soldier> soldierPtr, Soldier* soldier);
    void createPage() override;
    void setStatusText(const char* text);
    void setOnStartCallback(std::function<void()> cb);

    // Internal methods
    void startProcess();
    void poll();

    // LVGL button event handler
    static void startBtnEvent(lv_event_t* e) {
        auto* self = static_cast<DiffieHellmanPageSoldier*>(lv_event_get_user_data(e));
        self->onButtonPressed();
    }

    void setOnTransferMainPage(std::function<void(std::unique_ptr<WifiModule>, std::unique_ptr<Soldier>)> cb);

protected:
    // after pressing the button check if the process is ok to start
    virtual void onButtonPressed() {
        if (!soldierProcessStarted) {
            startProcess();
        }
    }
};