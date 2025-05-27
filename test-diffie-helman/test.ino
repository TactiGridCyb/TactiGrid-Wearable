// #include <Arduino.h>
// #include <WiFi.h>
// #include <HTTPClient.h>
// #include <LilyGoLib.h>
// #include <LV_Helper.h>
// #include <lvgl.h>

// #include "../commander-DH/CommanderECDHHandler.h"
// #include "../commander-config/commander-config.h"
// #include "../wifi-connection/WifiModule.h"

// CryptoHelper crypto = CryptoHelper();
// CommanderConfigModule* commanderConfig = nullptr;
// CommanderECDHHandler* commander = nullptr;
// WifiModule wifi("*****", "******");

// bool processStarted = false;
// lv_obj_t* startBtn;

// void downloadConfig(const String& url, CommanderConfigModule*& config) {
//     WiFi.begin("*******", "*******");
//     Serial.print("Connecting to WiFi");
//     while (WiFi.status() != WL_CONNECTED) {
//         delay(500);
//         Serial.print(".");
//     }
//     Serial.println(" connected!");

//     HTTPClient http;
//     http.begin(url);
//     int code = http.GET();
//     if (code == 200) {
//         String json = http.getString();
//         config = new CommanderConfigModule(json);
//         Serial.println("✅ Commander config downloaded");
//     } else {
//         Serial.println("❌ Failed to download commander config");
//     }
//     http.end();
//     WiFi.disconnect();
// }

// void startProcess() {
//     // === Step 1: Download Commander Config ===
//     downloadConfig("http://192.168.1.141:5555/download/watch1", commanderConfig);

//      // **NEW**: initialize CryptoHelper with both soldier cert & CA
//     if (!crypto.loadFromCommanderConfig(*commanderConfig)) {
//         Serial.println("❌ Failed to init crypto");
//         return;
//     }

//     // === Step 2: Initialize Commander ===
//     commander = new CommanderECDHHandler(868.0, commanderConfig, crypto);
//     commander->begin();

//     // === Step 3: Start ECDH with soldier ID 1 ===
//     if (commander->startECDHExchange(1)) {
//         Serial.println("📡 Commander sent ECDH init");
//     } else {
//         Serial.println("❌ Commander failed to send ECDH init");
//     }
//     processStarted = true;
// }

// void onButtonClicked(lv_event_t* e) {
//     if (!processStarted) {
//         startProcess();
//     }
// }

// void setup() {
//     Serial.begin(115200);
//     delay(1000);
//     watch.begin();
//     beginLvglHelper();

//     startBtn = lv_btn_create(lv_scr_act());
//     lv_obj_center(startBtn);
//     lv_obj_set_size(startBtn, 120, 50);
//     lv_obj_add_event_cb(startBtn, onButtonClicked, LV_EVENT_CLICKED, NULL);

//     lv_obj_t* label = lv_label_create(startBtn);
//     lv_label_set_text(label, "Start ECDH");
//     lv_obj_center(label);
// }

// void loop() {
//     lv_timer_handler();  // Run LVGL handler
//     delay(50);

//      // pump LoRa
//     commander->poll();

//     if (processStarted && commander->isExchangeComplete()) {
//         std::vector<uint8_t> shared = commander->getSharedSecret();
//         Serial.print("🔐 Commander Shared Secret: ");
//         for (auto b : shared) Serial.printf("%02X", b);
//         Serial.println();
//         processStarted = false;
//     }
// }















// ===================================================================
// SoldierTest.cpp
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <lvgl.h>

#include "../soldier-DH/SoldierECDHHandler.h"
#include "../soldier-config/soldier-config.h"
#include "../wifi-connection/WifiModule.h"
#include "../crypto-helper/crypto-helper.h"

CryptoHelper crypto = CryptoHelper();
SoldierConfigModule* soldierConfig = nullptr;
SoldierECDHHandler* soldier = nullptr;
WifiModule wifiSoldier("******", "******");

bool soldierProcessStarted = false;
lv_obj_t* startBtn;

void downloadSoldierConfig(const String& url, SoldierConfigModule*& config) {
    WiFi.begin("******", "******");
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println(" connected!");

    HTTPClient http;
    http.begin(url);
    int code = http.GET();
    if (code == 200) {
        String json = http.getString();
        config = new SoldierConfigModule(json);
        Serial.println("✅ Soldier config downloaded");
    } else {
        Serial.println("❌ Failed to download soldier config");
    }
    http.end();
    WiFi.disconnect();
}

void startSoldierProcess() {
    // === Step 1: Download Soldier Config ===
    downloadSoldierConfig("http://192.168.1.141:5555/download/watch2", soldierConfig);

     // **NEW**: initialize CryptoHelper with both soldier cert & CA
    if (!crypto.loadFromSoldierConfig(*soldierConfig)) {
        Serial.println("❌ Failed to init crypto");
        return;
    }

    // === Step 2: Initialize Soldier ===
    soldier = new SoldierECDHHandler(868.0, soldierConfig, crypto);
    soldier->begin();
    soldier->startListening();

    soldierProcessStarted = true;
}

void onButtonClicked(lv_event_t* e) {
    if (!soldierProcessStarted) {
        startSoldierProcess();
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    watch.begin();
    beginLvglHelper();

    startBtn = lv_btn_create(lv_scr_act());
    lv_obj_center(startBtn);
    lv_obj_set_size(startBtn, 120, 50);
    lv_obj_add_event_cb(startBtn, onButtonClicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(startBtn);
    lv_label_set_text(label, "Start Soldier");
    lv_obj_center(label);
}

void loop() {
    lv_timer_handler();  // Run LVGL handler
    delay(50);

     // pump LoRa
    soldier->poll();

    if (soldierProcessStarted && soldier->hasRespondedToCommander()) {
        std::vector<uint8_t> shared = soldier->getSharedSecret();
        Serial.print("🔐 Soldier Shared Secret: ");
        for (auto b : shared) Serial.printf("%02X", b);
        Serial.println();
        soldierProcessStarted = false;
    }
}
