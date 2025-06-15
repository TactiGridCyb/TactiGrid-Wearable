// DiffieHellmanPage.cpp
#include "diffieHelmanPageCommander.h"
#include <lvgl.h>
#include <Arduino.h>


DiffieHellmanPageCommander::DiffieHellmanPageCommander(Commander* commander)
    : page(nullptr), statusLabel(nullptr), startBtn(nullptr), onStart(nullptr), commanderProcessStarted(false), commander(commander) {
    }

// create page function
void DiffieHellmanPageCommander::createPage() {
    // Create a new screen and load it
    page = lv_obj_create(nullptr);
    lv_scr_load(page);

    // Black background
    lv_obj_set_style_bg_color(page, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Status label at the top
    statusLabel = lv_label_create(page);
    lv_label_set_text(statusLabel, "Ready for DH");
    lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_text_color(statusLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Start button
    startBtn = lv_btn_create(page);
    lv_obj_set_size(startBtn, 160, 50);
    lv_obj_align(startBtn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(startBtn, startBtnEvent, LV_EVENT_CLICKED, this);

    // Button label
    lv_obj_t* label = lv_label_create(startBtn);
    lv_label_set_text(label, "Start Diffie Hellman");
    lv_obj_center(label);

    // Create a 100ms LVGL timer to call poll()
    lv_timer_create([](lv_timer_t* t) {
        auto* self = static_cast<DiffieHellmanPageCommander*>(t->user_data);
        self->poll();
    }, 100, this);
}

// function to change the label on the watch
void DiffieHellmanPageCommander::setStatusText(const char* text) {
    if (statusLabel) {
        lv_label_set_text(statusLabel, text);
    }
}

// in order to move between pages
void DiffieHellmanPageCommander::setOnStartCallback(std::function<void()> cb) {
    onStart = std::move(cb);
}


void DiffieHellmanPageCommander::startProcess() {
    //TODO: understand where Soldier soldier comes from..?
    setStatusText("starting process...");

    setStatusText("Initializing crypto...");
    if (!certmodule.loadFromCommander(*commander)) {
        setStatusText("Crypto init failed");
        return;
    }

    setStatusText("Starting DH handler...");
    //add frequency
    dhHandler = new CommanderECDHHandler(868, commander, certmodule);
    dhHandler->begin();
    dhHandler->startECDHExchange(1);
    
    commanderProcessStarted = true;
    setStatusText("Waiting for soldier...");
}

void DiffieHellmanPageCommander::poll() {
    if (!commanderProcessStarted || !commander) return;

    dhHandler->poll();
    if (dhHandler->isExchangeComplete()) {
        // Build hex string of shared secret
        setStatusText("extracting shared secret..");
        auto shared = dhHandler->getSharedSecret();
        String b64 = certModule::toBase64(shared);
        Serial.print("Shared secret (Base64): ");
        Serial.println(b64);

        commanderProcessStarted = false;
    }
}