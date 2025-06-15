// DiffieHellmanPage.h
#pragma once
#include <LVGLPage.h>
#include <functional>
#include "../certModule/certModule.h"
#include "../LoraModule/LoraModule.h"
#include "../commander-DH/CommanderECDHHandler.h"

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

public:
    DiffieHellmanPageCommander(Commander* commander);
    void createPage() override;
    void setStatusText(const char* text);
    void setOnStartCallback(std::function<void()> cb);

    // Internal methods
    void startProcess();
    void poll();

    // LVGL button event handler
    static void startBtnEvent(lv_event_t* e) {
        auto* self = static_cast<DiffieHellmanPageCommander*>(lv_event_get_user_data(e));
        self->onButtonPressed();
    }

protected:
    // after pressing the button check if the process is ok to start
    virtual void onButtonPressed() {
        if (!commanderProcessStarted) {
            startProcess();
        }
    }
};
