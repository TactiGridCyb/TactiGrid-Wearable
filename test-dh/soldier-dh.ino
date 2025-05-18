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
  String certBase64 = config->getCertificatePEM();
  const int MAX_PAYLOAD = 220;  // conservative for LoRa

  // Estimate room used by metadata JSON structure
  const int JSON_OVERHEAD = 80;  // {"ts":123,"id":1,"part":9,"total_parts":99,...}

  // Chunk size: what's left for the actual cert chunk
  int chunkSize = MAX_PAYLOAD - JSON_OVERHEAD;

  // Split certificate
  std::vector<String> chunks;
  for (size_t i = 0; i < certBase64.length(); i += chunkSize) {
    chunks.push_back(certBase64.substring(i, i + chunkSize));
  }

  Serial.printf("📤 Sending certificate in %d chunks to soldier %d...\n", chunks.size(), soldierId);

  for (int i = 0; i < chunks.size(); ++i) {
    DynamicJsonDocument doc(1024);
    doc["ts"] = millis();
    doc["id"] = soldierId;
    doc["part"] = i;
    doc["total_parts"] = chunks.size();
    doc["cert_chunk"] = chunks[i];

    String out;
    serializeJson(doc, out);

    Serial.printf("LoRa TX chunk %d/%d (%d bytes)\n", i + 1, chunks.size(), out.length());
    lora.sendData(out.c_str());
    lora.cleanUpTransmissions();
    delay(3500);  // prevent congestion
  }
}

void onStartMissionPressed(lv_event_t* e) {
  Serial.println("[Commander] Start Mission pressed.");
  lv_label_set_text(statusLabel, "Sending to soldier 1");
  sendCertificateToSoldier(1);  // Always send to soldier ID 1
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



// //soldier
// #include <LilyGoLib.h>
// #include "../LoraModule/LoraModule.h"
// #include "../commander-config/commander-config.h"
// #include "crypto-helper.h"
// #include "../wifi-connection/WifiModule.h"
// #include <LV_Helper.h>
// #include <ArduinoJson.h>
// #include <FFat.h>
// #include <map>
// #include <vector>
// #include <HTTPClient.h>
// #include "mbedtls/base64.h"
// #include "mbedtls/error.h"

// // Modules
// LoraModule lora(868.0);
// CommanderConfigModule* config = nullptr;
// CryptoHelper crypto;
// WifiModule wifi("peretz1", "box17box");

// // UI
// lv_obj_t* statusLabel;
// lv_obj_t* downloadBtn;
// lv_obj_t* waitForCertBtn;

// // LoRa reception
// std::map<int, String> receivedChunks;
// int totalParts = -1;
// int senderId = -1;
// unsigned long lastReceiveTime = 0;
// const unsigned long RECEIVE_TIMEOUT = 60000;
// bool listening = false;

// // Forward declarations
// void onDownloadPressed(lv_event_t* e);
// void onWaitForCertPressed(lv_event_t* e);
// void onLoRaData(const String& data);
// bool tryReconstructAndVerify();

// void setupUI() {
//   statusLabel = lv_label_create(lv_scr_act());
//   lv_label_set_text(statusLabel, "Ready to download config");
//   lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 20);

//   downloadBtn = lv_btn_create(lv_scr_act());
//   lv_obj_set_size(downloadBtn, 160, 50);
//   lv_obj_align(downloadBtn, LV_ALIGN_CENTER, 0, -40);
//   lv_obj_add_event_cb(downloadBtn, onDownloadPressed, LV_EVENT_CLICKED, NULL);
//   lv_obj_t* label1 = lv_label_create(downloadBtn);
//   lv_label_set_text(label1, "Download Config");
//   lv_obj_center(label1);

//   waitForCertBtn = lv_btn_create(lv_scr_act());
//   lv_obj_set_size(waitForCertBtn, 160, 50);
//   lv_obj_align(waitForCertBtn, LV_ALIGN_CENTER, 0, 40);
//   lv_obj_add_event_cb(waitForCertBtn, onWaitForCertPressed, LV_EVENT_CLICKED, NULL);
//   lv_obj_t* label2 = lv_label_create(waitForCertBtn);
//   lv_label_set_text(label2, "Wait for Cert");
//   lv_obj_center(label2);
//   lv_obj_add_flag(waitForCertBtn, LV_OBJ_FLAG_HIDDEN);
// }

// void onDownloadPressed(lv_event_t* e) {
//   wifi.connect(10000);
//   if (!wifi.isConnected()) {
//     lv_label_set_text(statusLabel, "WiFi failed");
//     return;
//   }

//   HTTPClient http;
//   http.begin("http://192.168.1.141:5555/download/watch2");
//   int httpCode = http.GET();
//   if (httpCode == 200) {
//     String payload = http.getString();
//     config = new CommanderConfigModule(payload);
//     if (!crypto.loadFromConfig(*config)) {
//       lv_label_set_text(statusLabel, "Crypto error");
//       return;
//     }
//     lv_label_set_text(statusLabel, "Download complete");
//     lv_obj_clear_flag(waitForCertBtn, LV_OBJ_FLAG_HIDDEN);
//   } else {
//     lv_label_set_text(statusLabel, "Download failed");
//   }
//   http.end();
//   wifi.disconnect();
// }

// void onWaitForCertPressed(lv_event_t* e) {
//   lv_label_set_text(statusLabel, "Listening for certificate...");
//   receivedChunks.clear();
//   totalParts = -1;
//   senderId = -1;
//   listening = true;
//   lora.setOnReadData(onLoRaData);
//   lora.setupListening();
// }

// void onLoRaData(const String& msg) {
//   if (!listening) return;

//   DynamicJsonDocument doc(1024);
//   if (deserializeJson(doc, msg)) return;

//   int part = doc["part"];
//   int total = doc["total_parts"];
//   int id = doc["id"];
//   String chunk = doc["cert_chunk"].as<String>();

//   if (config && id != config->getId()) return;

//   if (senderId == -1) {
//     senderId = id;
//     totalParts = total;
//   }

//   receivedChunks[part] = chunk;
//   lastReceiveTime = millis();

//   Serial.printf("Chunk %d of %d received\n", receivedChunks.size(), totalParts);

//   if ((int)receivedChunks.size() == totalParts) {
//     Serial.println("✅ All chunks received. Verifying...");
//     if (tryReconstructAndVerify()) {
//       listening = false;
//     }
//   }
// }

// bool tryReconstructAndVerify() {
//   String fullBase64;
//   for (int i = 0; i < totalParts; ++i) fullBase64 += receivedChunks[i];

//   std::vector<uint8_t> decoded((fullBase64.length() * 3) / 4 + 2);
//   size_t decodedLen;
//   int ret = mbedtls_base64_decode(decoded.data(), decoded.size(), &decodedLen,
//                                   (const uint8_t*)fullBase64.c_str(), fullBase64.length());
//   if (ret != 0) {
//     lv_label_set_text(statusLabel, "Base64 decode failed");
//     Serial.println("❌ Base64 decode failed");
//     return false;
//   }

//   decoded[decodedLen] = '\0';
//   const char* pem = (const char*)decoded.data();

//   mbedtls_x509_crt commanderCert;
//   mbedtls_x509_crt_init(&commanderCert);
//   ret = mbedtls_x509_crt_parse(&commanderCert, (const uint8_t*)pem, decodedLen + 1);
//   if (ret != 0) {
//     lv_label_set_text(statusLabel, "Cert parse failed");
//     Serial.println("❌ Cert parse failed");
//     return false;
//   }

//   mbedtls_x509_crt caCert;
//   mbedtls_x509_crt_init(&caCert);

//   String caBase64 = config->getCaCertificatePEM();
//   std::vector<uint8_t> caDecoded((caBase64.length() * 3) / 4 + 2);
//   size_t caDecodedLen;

//   ret = mbedtls_base64_decode(caDecoded.data(), caDecoded.size(), &caDecodedLen,
//                               (const uint8_t*)caBase64.c_str(), caBase64.length());

//   if (ret != 0) {
//     lv_label_set_text(statusLabel, "CA decode failed");
//     Serial.println("❌ CA decode failed");
//     return false;
//   }

//   caDecoded[caDecodedLen] = '\0';
//   ret = mbedtls_x509_crt_parse(&caCert, caDecoded.data(), caDecodedLen + 1);
//   if (ret != 0) {
//     lv_label_set_text(statusLabel, "CA parse failed");
//     Serial.println("❌ CA parse failed");
//     return false;
//   }

//   ret = mbedtls_x509_crt_verify(&commanderCert, &caCert, nullptr, nullptr, nullptr, nullptr, nullptr);

//   if (ret == 0) {
//     lv_label_set_text(statusLabel, "Certificate: VALID ✅");
//     Serial.println("✅ Certificate: VALID");
//     return true;
//   } else {
//     lv_label_set_text(statusLabel, "Certificate: INVALID ❌");
//     Serial.println("❌ Certificate: INVALID");
//     return false;
//   }
// }

// void setup() {
//   Serial.begin(115200);
//   watch.begin();
//   beginLvglHelper();
//   setupUI();
//   lora.setup(false);
// }

// void loop() {
//   lv_timer_handler();
//   delay(10);

//   lora.readData();

//   if (listening && lastReceiveTime > 0 && millis() - lastReceiveTime > RECEIVE_TIMEOUT) {
//     lv_label_set_text(statusLabel, "Reception timed out");
//     Serial.println("⏱️ Reception timed out");
//     receivedChunks.clear();
//     totalParts = -1;
//     senderId = -1;
//     lastReceiveTime = 0;
//   }
// }
