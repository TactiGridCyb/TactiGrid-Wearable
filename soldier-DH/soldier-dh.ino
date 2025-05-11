// // SoldierMissionStart.ino
// #include <LilyGoLib.h>
// #include "../LoraModule/LoraModule.h"
// #include "../commander-config/commander-config.h"
// #include "crypto-helper.h"
// #include <LV_Helper.h>
// #include <ArduinoJson.h>
// #include <FFat.h>

// // LoRa module on default freq
// LoraModule lora(868.0);

// // Config and crypto modules
// CommanderConfigModule* config = nullptr;
// CryptoHelper crypto;

// // Soldier ID
// int soldierId = -1;  // Will be loaded from config

// // UI
// lv_obj_t* statusLabel;
// lv_obj_t* startMissionBtn;

// // Cert chunking state
// String receivedCertData = "";
// int totalPartsExpected = 0;
// int partsReceived = 0;
// bool commanderVerified = false;

// void FHF() {
//   Serial.println("[FHF] Soldier init logic called.");
// }

// void calculate_GK() {
//   Serial.println("[calculate_GK] Called (placeholder)");
// }

// void onStartMissionPressed(lv_event_t* e) {
//   Serial.println("[UI] Start Mission pressed");
//   lv_label_set_text(statusLabel, "Mission started ✔");
//   calculate_GK();
// }

// void setupUI() {
//   statusLabel = lv_label_create(lv_scr_act());
//   lv_label_set_text(statusLabel, "Waiting for commander...");
//   lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 20);

//   startMissionBtn = lv_btn_create(lv_scr_act());
//   lv_obj_set_size(startMissionBtn, 160, 50);
//   lv_obj_align(startMissionBtn, LV_ALIGN_BOTTOM_MID, 0, -20);
//   lv_obj_add_event_cb(startMissionBtn, onStartMissionPressed, LV_EVENT_CLICKED, NULL);
//   lv_obj_t* label = lv_label_create(startMissionBtn);
//   lv_label_set_text(label, "Start Mission");
//   lv_obj_center(label);
//   lv_obj_add_flag(startMissionBtn, LV_OBJ_FLAG_HIDDEN);  // Hide initially
// }

// void setup() {
//   Serial.begin(115200);
//   watch.begin();
//   beginLvglHelper();

//   if (!FFat.begin()) {
//     Serial.println("❌ FFat mount failed");
//     return;
//   }

//   File f = FFat.open("/commander_config.json", "r");
//   if (!f) {
//     Serial.println("❌ Cannot open config file");
//     return;
//   }

//   String content;
//   while (f.available()) content += (char)f.read();
//   f.close();

//   config = new CommanderConfigModule(content);
//   soldierId = config->getId();

//   if (!crypto.loadFromConfig(*config)) {
//     Serial.println("❌ Failed to load crypto materials");
//     return;
//   }

//   FHF();
//   setupUI();

//   if (lora.setup(false) != RADIOLIB_ERR_NONE) {
//     Serial.println("❌ Failed to start LoRa receive mode");
//   } else {
//     lora.setOnReadData(onLoRaMessage);
//     lora.setupListening();
//     Serial.println("✅ Soldier is now listening on LoRa");
//   }
// }

// void loop() {
//   lora.readData();
//   lv_timer_handler();
//   delay(10);
// }

// void onLoRaMessage(const String& data) {
//   Serial.println("📨 Received message:");
//   Serial.println(data);

//   DynamicJsonDocument doc(2048);
//   DeserializationError err = deserializeJson(doc, data);
//   if (err) {
//     Serial.println("❌ Failed to parse message");
//     return;
//   }

//   int recipientId = doc["id"];
//   if (recipientId != soldierId) {
//     Serial.printf("Message for soldier ID %d, I am %d — skipping\n", recipientId, soldierId);
//     return;
//   }

//   const char* chunk = doc["cert_chunk"];
//   int part = doc["part"];
//   int total = doc["total_parts"];

//   if (!chunk || total <= 0) {
//     Serial.println("❌ Invalid chunked certificate message");
//     return;
//   }

//   if (part == 0) {
//     receivedCertData = "";
//     totalPartsExpected = total;
//     partsReceived = 0;
//   }

//   receivedCertData += String(chunk);
//   partsReceived++;

//   Serial.printf("✅ Received part %d/%d\n", part + 1, total);

//   if (partsReceived == totalPartsExpected) {
//     Serial.println("📦 All parts received. Verifying commander...");

//     if (!crypto.loadSingleCertificate(receivedCertData)) {
//       Serial.println("❌ Failed to load commander cert");
//       return;
//     }

//     if (!crypto.verifyCertificate()) {
//       Serial.println("❌ Commander certificate not valid");
//       lv_label_set_text(statusLabel, "Invalid commander cert");
//       return;
//     }

//     Serial.println("✅ Commander verified. Soldier ready.");
//     lv_label_set_text(statusLabel, "Commander verified ✔");
//     lv_obj_clear_flag(startMissionBtn, LV_OBJ_FLAG_HIDDEN);
//     commanderVerified = true;
//   }
// }


//commander
// CommanderMissionStart.ino
#include <LilyGoLib.h>
#include "../LoraModule/LoraModule.h"
#include "../commander-config/commander-config.h"
#include "crypto-helper.h"
#include <LV_Helper.h>
#include <ArduinoJson.h>
#include <FFat.h>
#include <WiFi.h>
#include <HTTPClient.h>

// LoRa module on default freq
LoraModule lora(868.0);

// Config and crypto modules
CommanderConfigModule* config = nullptr;
CryptoHelper crypto;

// Soldier list from config
std::vector<int> soldierIds;
int currentSoldierIndex = 0;

// UI
lv_obj_t* statusLabel;
lv_obj_t* startMissionBtn;
lv_obj_t* downloadBtn;

// Certificate splitting
std::vector<String> certChunks;
const int CHUNK_SIZE = 300;

// WiFi credentials
const char* ssid = "peretz1";
const char* password = "box17box";
const char* downloadURL = "http://192.168.1.141:5555/download/watch1";

void FHF() {
  Serial.println("[FHF] Commander init logic called.");
}

void splitCertificate(const String& certBase64) {
  certChunks.clear();
  for (size_t i = 0; i < certBase64.length(); i += CHUNK_SIZE) {
    certChunks.push_back(certBase64.substring(i, i + CHUNK_SIZE));
  }
}

void sendCertificateToSoldier(int soldierId) {
  Serial.printf("Sending to soldier ID %d:\n", soldierId);
  for (int i = 0; i < certChunks.size(); ++i) {
    DynamicJsonDocument doc(1024);
    doc["ts"] = millis();
    doc["id"] = soldierId;
    doc["part"] = i;
    doc["total_parts"] = certChunks.size();
    doc["cert_chunk"] = certChunks[i];

    String out;
    serializeJson(doc, out);
    lora.sendData(out.c_str());
    lora.cleanUpTransmissions();
    delay(300);
  }
}

void onStartMissionPressed(lv_event_t* e) {
  Serial.println("[Commander] Start Mission pressed.");
  lv_label_set_text(statusLabel, "Mission initiated");

  if (currentSoldierIndex < soldierIds.size()) {
    sendCertificateToSoldier(soldierIds[currentSoldierIndex]);
    currentSoldierIndex++;
  } else {
    lv_label_set_text(statusLabel, "All soldiers initiated");
  }
}

void onDownloadPressed(lv_event_t* e) {
  lv_label_set_text(statusLabel, "Connecting to WiFi...");
  WiFi.begin(ssid, password);

  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n❌ WiFi connection failed");
    lv_label_set_text(statusLabel, "WiFi failed");
    return;
  }

  Serial.println("\n✅ WiFi connected");
  HTTPClient http;
  http.begin(downloadURL);
  int httpCode = http.GET();

  if (httpCode == 200) {
    String payload = http.getString();
    Serial.println("✅ Received and parsed config");

    config = new CommanderConfigModule(payload);
    soldierIds = config->getSoldiers();

    if (!crypto.loadFromConfig(*config)) {
      Serial.println("❌ Failed to load crypto materials");
      return;
    }

    splitCertificate(config->getCertificatePEM());
    lv_label_set_text(statusLabel, "Download complete");
    lv_obj_clear_flag(startMissionBtn, LV_OBJ_FLAG_HIDDEN);
  } else {
    Serial.printf("❌ HTTP error: %d\n", httpCode);
    lv_label_set_text(statusLabel, "Download failed");
  }

  http.end();
  WiFi.disconnect(true, true);
}

void setupUI() {
  statusLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(statusLabel, "Ready to receive config");
  lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 20);

  downloadBtn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(downloadBtn, 160, 50);
  lv_obj_align(downloadBtn, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_event_cb(downloadBtn, onDownloadPressed, LV_EVENT_CLICKED, NULL);
  lv_obj_t* label1 = lv_label_create(downloadBtn);
  lv_label_set_text(label1, "Receive Config");
  lv_obj_center(label1);

  startMissionBtn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(startMissionBtn, 160, 50);
  lv_obj_align(startMissionBtn, LV_ALIGN_BOTTOM_MID, 0, -20);
  lv_obj_add_event_cb(startMissionBtn, onStartMissionPressed, LV_EVENT_CLICKED, NULL);
  lv_obj_t* label2 = lv_label_create(startMissionBtn);
  lv_label_set_text(label2, "Start Mission");
  lv_obj_center(label2);
  lv_obj_add_flag(startMissionBtn, LV_OBJ_FLAG_HIDDEN);
}

void setup() {
  Serial.begin(115200);
  watch.begin();
  beginLvglHelper();
  FHF();
  setupUI();

  if (lora.setup(true) != RADIOLIB_ERR_NONE) {
    Serial.println("❌ Failed to start LoRa transmit mode");
  } else {
    Serial.println("✅ Commander ready to transmit via LoRa");
  }
}

void loop() {
  lv_timer_handler();
  delay(10);
}
