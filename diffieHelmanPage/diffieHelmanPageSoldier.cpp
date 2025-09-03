// DiffieHellmanPage.cpp
#include "diffieHelmanPageSoldier.h"
#include <lvgl.h>
#include <Arduino.h>


DiffieHellmanPageSoldier::DiffieHellmanPageSoldier(Soldier* soldier)
    : page(nullptr), statusLabel(nullptr), startBtn(nullptr), onStart(nullptr), soldierProcessStarted(false), soldier(soldier) {
    }

// create page function
void DiffieHellmanPageSoldier::createPage() {
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
        auto* self = static_cast<DiffieHellmanPageSoldier*>(t->user_data);
        self->poll();
    }, 100, this);
}

// function to change the label on the watch
void DiffieHellmanPageSoldier::setStatusText(const char* text) {
    if (statusLabel) {
        lv_label_set_text(statusLabel, text);
    }
}

// in order to move between pages
void DiffieHellmanPageSoldier::setOnStartCallback(std::function<void()> cb) {
    onStart = std::move(cb);
}


void DiffieHellmanPageSoldier::startProcess() {
    setStatusText("Initializing crypto...");
    if (!certmodule.loadFromSoldier(*soldier)) {
      setStatusText("Crypto init failed");
      return;
    }

    setStatusText("Starting DH handler...");
    // allocate *your* member, not a new local
    dhHandler = new SoldierECDHHandler(868, soldier, certmodule);
    dhHandler->begin();
    dhHandler->startListening();

    soldierProcessStarted = true;
    setStatusText("Waiting for commander...");
}


void DiffieHellmanPageSoldier::poll() {
  if (!soldierProcessStarted || !dhHandler) return;
  dhHandler->poll();
  if (dhHandler->hasRespondedToCommander() && dhHandler->hasReceivedSecureMessage()) {
    // Build hex string of shared secret
        setStatusText("extracting shared secret..");
        auto shared = dhHandler->getSharedSecret();
        String b64 = certModule::toBase64(shared);
        Serial.print("Shared secret (Base64): ");
        Serial.println(b64);

        Serial.print("Final message (String): ");
        Serial.println(dhHandler->getFinalMessage());
        soldierProcessStarted = false;
  }
}