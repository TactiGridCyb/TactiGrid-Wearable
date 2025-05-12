#include <LilyGoLib.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "../commander-config/commander-config.h"
#include "crypto-helper.h"
#include <LV_Helper.h>

// WiFi + URL
const char* ssid = "peretz1";
const char* password = "box17box";
const char* configURL = "http://192.168.1.141:5555/download/watch1";

CommanderConfigModule* config = nullptr;
CryptoHelper crypto;

// UI Elements
lv_obj_t* statusLabel;
lv_obj_t* downloadBtn;

// Callback for download + verify
void onDownloadPressed(lv_event_t* e) {
  lv_label_set_text(statusLabel, "Connecting to WiFi...");
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    lv_label_set_text(statusLabel, "WiFi failed ❌");
    Serial.println("❌ WiFi failed");
    return;
  }

  Serial.println("\n✅ WiFi connected");
  lv_label_set_text(statusLabel, "Downloading config...");

  HTTPClient http;
  http.begin(configURL);
  int httpCode = http.GET();

  if (httpCode != 200) {
    lv_label_set_text(statusLabel, "Download failed ❌");
    Serial.printf("❌ HTTP %d\n", httpCode);
    return;
  }

  String payload = http.getString();
  Serial.println("✅ Config downloaded");
  config = new CommanderConfigModule(payload);

  if (!crypto.loadFromConfig(*config)) {
    lv_label_set_text(statusLabel, "Load error ❌");
    Serial.println("❌ Failed to load crypto material");
    return;
  }

  if (crypto.verifyCertificate()) {
    lv_label_set_text(statusLabel, "✅ Certificate VALID");
    Serial.println("✅ Certificate is VALID");
  } else {
    lv_label_set_text(statusLabel, "❌ Certificate INVALID");
    Serial.println("❌ Certificate is INVALID");
  }

  http.end();
  WiFi.disconnect(true, true);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  watch.begin();
  beginLvglHelper();

  // UI
  statusLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(statusLabel, "Ready");
  lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 20);

  downloadBtn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(downloadBtn, 160, 50);
  lv_obj_align(downloadBtn, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_event_cb(downloadBtn, onDownloadPressed, LV_EVENT_CLICKED, NULL);
  lv_obj_t* label = lv_label_create(downloadBtn);
  lv_label_set_text(label, "Verify Cert");
  lv_obj_center(label);
}

void loop() {
  lv_timer_handler();
  delay(10);
}
