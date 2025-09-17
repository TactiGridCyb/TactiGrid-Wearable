// *****************************************************************************************
// Soldier ECDH Receiver - Handles certificate verification and ECDH secure key agreement
// *****************************************************************************************

#include <LilyGoLib.h>
#include "../LoraModule/LoraModule.h"
#include "../commander-config/commander-config.h"
#include "certModule.h"
#include "ECDHHelper.h"
#include "../wifi-connection/WifiModule.h"
#include <LV_Helper.h>
#include <ArduinoJson.h>
#include <FFat.h>
#include <HTTPClient.h>
#include <vector>
#include "mbedtls/base64.h"

// === Module Instances ===
LoraModule lora(868.0);                          // LoRa radio on 868 MHz
CommanderConfigModule* config = nullptr;        // Configuration received from server
certModule crypto;                            // Handles cert + key parsing and verification
ECDHHelper ecdh;                                // Handles ephemeral ECDH operations
WifiModule wifi("peretz1", "box17box");         // Connects to web server to download config

// === UI Elements ===
lv_obj_t* statusLabel;
lv_obj_t* downloadBtn;
lv_obj_t* startMissionBtn;

bool hasResponded = false;


// === Commander ECDH data reception ===
String incomingMessage;
bool receiving = false;
unsigned long lastReceiveTime = 0;
const unsigned long TIMEOUT_MS = 10000;

// === Helper: base64 encode byte vector ===
String toBase64(const std::vector<uint8_t>& input) {
  size_t outLen = 4 * ((input.size() + 2) / 3);
  std::vector<uint8_t> outBuf(outLen + 1); // +1 for null terminator
  size_t actualLen = 0;

  int ret = mbedtls_base64_encode(outBuf.data(), outBuf.size(), &actualLen,
                                   input.data(), input.size());

  if (ret != 0) {
    Serial.println("❌ Base64 encode failed");
    return "";
  }

  outBuf[actualLen] = '\0';
  return String((const char*)outBuf.data());
}

// === Helper: Decode base64 to byte vector ===
bool decodeBase64(const String& input, std::vector<uint8_t>& output) {
  size_t inputLen = input.length();
  size_t decodedLen = (inputLen * 3) / 4 + 2;
  output.resize(decodedLen);
  size_t outLen = 0;
  int ret = mbedtls_base64_decode(output.data(), output.size(), &outLen,
                                   (const uint8_t*)input.c_str(), inputLen);
  if (ret != 0) return false;
  output.resize(outLen);
  return true;
}

void sendECDHResponse(int toId) {
  Serial.println("📤 Sending soldier response...");

  // ✅ Step 1: Get the PEM string and fix line endings
  String certB64 = config->getCertificatePEM();
String ephB64 = toBase64(ecdh.getPublicKeyRaw());

  DynamicJsonDocument doc(4096);
  doc["id"] = config->getId();
  doc["cert"] = certB64;
  doc["ephemeral"] = ephB64;


  // ✅ Step 6: Serialize and send
  String out;
  serializeJsonPretty(doc, out);
  lora.sendFile((const uint8_t*)out.c_str(), out.length(), 180);
  lv_label_set_text(statusLabel, "✅ Response sent");
}




// === Handle incoming commander ECDH message ===
void onLoRaData(const uint8_t* data, size_t len) {
  if (hasResponded) {
    Serial.println("⚠️ Already responded, ignoring message");
    return;
  }   

  receiving = false;
  String msg((const char*)data, len);
  Serial.println("📥 Received commander message:");
  Serial.println(msg);

  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, msg) != DeserializationError::Ok) {
    lv_label_set_text(statusLabel, "❌ JSON error");
    return;
  }

  int recipientId = doc["id"];
  if (recipientId != config->getId()) {
    lv_label_set_text(statusLabel, "⚠️ Not for me");
    return;
  }

  // === Step 1: Verify commander's certificate ===
  String certB64 = doc["cert"].as<String>();
  std::vector<uint8_t> certRaw;
  if (!decodeBase64(certB64, certRaw)) {
    lv_label_set_text(statusLabel, "❌ Cert decode");
    return;
  }

  if (!crypto.loadSingleCertificate(String((const char*)certRaw.data()))) {
    lv_label_set_text(statusLabel, "❌ Cert parse");
    return;
  }

  if (!crypto.verifyCertificate()) {
    lv_label_set_text(statusLabel, "❌ Invalid cert");
    return;
  }

  Serial.println("✅ Commander cert valid");

  // === Step 2: Load and import commander's ephemeral key ===
  String ephB64 = doc["ephemeral"].as<String>();
  std::vector<uint8_t> ephRaw;
  if (!decodeBase64(ephB64, ephRaw)) {
    lv_label_set_text(statusLabel, "❌ E key decode");
    return;
  }

  if (!ecdh.importPeerPublicKey(ephRaw)) {
    lv_label_set_text(statusLabel, "❌ E key import");
    return;
  }

  // === Step 3: Generate own ephemeral key and compute shared secret ===
  if (!ecdh.generateEphemeralKey()) {
    lv_label_set_text(statusLabel, "❌ My keygen");
    return;
  }

  std::vector<uint8_t> shared;
  if (!ecdh.deriveSharedSecret(shared)) {
    lv_label_set_text(statusLabel, "❌ DH derive");
    return;
  }

  Serial.println("✅ Shared secret OK");
  lv_label_set_text(statusLabel, "✅ Secure!");

//✅ Mark as responded
hasResponded = true;

  // === Step 4: Respond back with our cert + ephemeral ===
  sendECDHResponse(doc["id"]);
}



// === Download config from Wi-Fi server ===
void onDownloadPressed(lv_event_t* e) {
  wifi.connect(10000);
  if (!wifi.isConnected()) {
    lv_label_set_text(statusLabel, "WiFi fail");
    return;
  }

  HTTPClient http;
  http.begin("http://192.168.1.141:5555/download/watch2");
  int httpCode = http.GET();
  if (httpCode == 200) {
    String payload = http.getString();
    config = new CommanderConfigModule(payload);
    if (!crypto.loadFromConfig(*config)) {
      lv_label_set_text(statusLabel, "Crypto error");
      return;
    }
    lv_label_set_text(statusLabel, "Download OK");
    lv_obj_clear_flag(startMissionBtn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text(statusLabel, "HTTP fail");
  }
  http.end();
  wifi.disconnect();
}

// === Start listening for commander ECDH init ===
void onStartMissionPressed(lv_event_t* e) {
  lv_label_set_text(statusLabel, "Waiting commander...");
  receiving = true;
  lastReceiveTime = millis();
  lora.setupListening();
  
  lora.setOnReadData([](const uint8_t* pkt, size_t len) {
    lora.onLoraFileDataReceived(pkt, len);  // ✅ handles reassembly
});
lora.onFileReceived = onLoRaData;  // ✅ called only when full file is ready

}

// === UI Setup ===
void setupUI() {
  statusLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(statusLabel, "Init soldier...");
  lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 20);

  downloadBtn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(downloadBtn, 160, 50);
  lv_obj_align(downloadBtn, LV_ALIGN_CENTER, 0, -40);
  lv_obj_add_event_cb(downloadBtn, onDownloadPressed, LV_EVENT_CLICKED, NULL);
  lv_obj_t* label1 = lv_label_create(downloadBtn);
  lv_label_set_text(label1, "Download Config");
  lv_obj_center(label1);

  startMissionBtn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(startMissionBtn, 160, 50);
  lv_obj_align(startMissionBtn, LV_ALIGN_CENTER, 0, 40);
  lv_obj_add_event_cb(startMissionBtn, onStartMissionPressed, LV_EVENT_CLICKED, NULL);
  lv_obj_t* label2 = lv_label_create(startMissionBtn);
  lv_label_set_text(label2, "Wait for Commander");
  lv_obj_center(label2);
  lv_obj_add_flag(startMissionBtn, LV_OBJ_FLAG_HIDDEN);
}

// === Main setup ===
void setup() {
  Serial.begin(115200);
  watch.begin();
  beginLvglHelper();
  setupUI();
  lora.setup(false);  // Receiver mode
lora.setupListening();

lora.setOnReadData([](const uint8_t* pkt, size_t len) {
    lora.onLoraFileDataReceived(pkt, len);
});

Serial.println("finish init message from commander");
lora.onFileReceived = onLoRaData;  // <-- THIS must call your JSON handler

}


// === Main loop ===
void loop() {
  lv_timer_handler();
  delay(10);
  lora.readData();

  if (receiving && millis() - lastReceiveTime > TIMEOUT_MS) {
    lv_label_set_text(statusLabel, "❌ Timeout");
    receiving = false;
  }
}















// // ***************************************************************************************
// // Commander ECDH Initiator - Sends certificate + ephemeral key, verifies soldier reply
// // ***************************************************************************************

#include <LilyGoLib.h>
#include "../LoraModule/LoraModule.h"
#include "../commander-config/commander-config.h"
#include "certModule.h"
#include "ECDHHelper.h"
#include "../wifi-connection/WifiModule.h"
#include <LV_Helper.h>
#include <ArduinoJson.h>
#include <FFat.h>
#include <HTTPClient.h>
#include <vector>
#include "mbedtls/base64.h"

// === Modules ===
LoraModule lora(868.0);
CommanderConfigModule* config = nullptr;
certModule crypto;
ECDHHelper ecdh;
WifiModule wifi("peretz1", "box17box");

// === UI ===
lv_obj_t* statusLabel;
lv_obj_t* downloadBtn;
lv_obj_t* startMissionBtn;

// === additional ===
bool hasHandledResponse = false;


// === Soldier response ===
bool waitingResponse = false;
unsigned long startWaitTime = 0;
const unsigned long TIMEOUT_MS = 15000;

// === Helper: base64 encode byte vector ===
String toBase64(const std::vector<uint8_t>& input) {
  size_t outLen = 4 * ((input.size() + 2) / 3);
  std::vector<uint8_t> outBuf(outLen + 1); // +1 for null terminator
  size_t actualLen = 0;

  int ret = mbedtls_base64_encode(outBuf.data(), outBuf.size(), &actualLen,
                                   input.data(), input.size());

  if (ret != 0) {
    Serial.println("❌ Base64 encode failed");
    return "";
  }

  outBuf[actualLen] = '\0';
  return String((const char*)outBuf.data());
}

// === Helper: Decode base64 to byte vector ===
bool decodeBase64(const String& input, std::vector<uint8_t>& output) {
  Serial.println("enter: decode function");
  size_t inputLen = input.length();
  size_t decodedLen = (inputLen * 3) / 4 + 2;
  output.resize(decodedLen);
  size_t outLen = 0;
  Serial.println("enter: decode function1");
  int ret = mbedtls_base64_decode(output.data(), output.size(), &outLen,
                                   (const uint8_t*)input.c_str(), inputLen);
  Serial.println("enter: decode function2");

  if (ret != 0) {
    char errBuf[128];
    mbedtls_strerror(ret, errBuf, sizeof(errBuf));
    Serial.printf("❌ Base64 decode failed: -0x%04X (%s)\n", -ret, errBuf);
    return false;
  }

  output.resize(outLen);
  Serial.println("enter: decode function3");
  return true;
}



// === Start mission pressed ===
void onStartPressed(lv_event_t* e) {
  if (!ecdh.generateEphemeralKey()) {
    lv_label_set_text(statusLabel, "❌ E keygen fail");
    return;
  }

  // === Send message to first soldier ===
  int soldierId = config->getSoldiers().front();

  String certB64 = config->getCertificatePEM(); //added fixed raw cert
  String ephB64 = toBase64(ecdh.getPublicKeyRaw());

  DynamicJsonDocument doc(4096);
  doc["id"] = soldierId;
  doc["cert"] = certB64;
  doc["ephemeral"] = ephB64;

  String out;
  serializeJson(doc, out);
  int16_t result = lora.sendFile((const uint8_t*)out.c_str(), out.length(), 180);
if (result == RADIOLIB_ERR_NONE) {
  lv_label_set_text(statusLabel, "📤 Sent init, waiting...");
  waitingResponse = true;
  startWaitTime = millis();
  lora.setupListening(); // Explicitly set LoRa back to receive mode
} else {
  lv_label_set_text(statusLabel, "❌ Send failed");
}

}
//****************************************************************************************************************** */
void onLoRaData(const uint8_t* data, size_t len) {

  if (hasHandledResponse) {
  Serial.println("⚠️ Already handled response");
  return;
}

Serial.println("🔄 Processing soldier response...");


  String msg((const char*)data, len);
  Serial.println("📥 Received soldier response:");
  Serial.println(msg);

  DynamicJsonDocument doc(4096);
  if (deserializeJson(doc, msg) != DeserializationError::Ok) {
    lv_label_set_text(statusLabel, "❌ JSON error");
    return;
  }

  int soldierId = doc["id"];
  if (std::find(config->getSoldiers().begin(), config->getSoldiers().end(), soldierId) == config->getSoldiers().end()) {
    lv_label_set_text(statusLabel, "⚠️ Invalid soldier");
    return;
  }
  Serial.println("A");
// === Step 1: Verify commander's certificate ===
  String certB64 = doc["cert"].as<String>();
  std::vector<uint8_t> certRaw;
  if (!decodeBase64(certB64, certRaw)) {
    lv_label_set_text(statusLabel, "❌ Cert decode");
    return;
  }
  Serial.println("B");

  if (!crypto.loadSingleCertificate(String((const char*)certRaw.data()))) {
    lv_label_set_text(statusLabel, "❌ Cert parse");
    return;
  }
  Serial.println("C");

  if (!crypto.verifyCertificate()) {
    lv_label_set_text(statusLabel, "❌ Invalid cert");
    return;
  }
  Serial.println("D");

Serial.println("✅ Soldier cert valid");


  // === calculate shared secret ===
  std::vector<uint8_t> ephRaw;
  if (!decodeBase64(doc["ephemeral"], ephRaw)) {
    lv_label_set_text(statusLabel, "❌ E decode");
    return;
  }

  if (!ecdh.importPeerPublicKey(ephRaw)) {
    lv_label_set_text(statusLabel, "❌ E import");
    return;
  }

  std::vector<uint8_t> shared;
  if (!ecdh.deriveSharedSecret(shared)) {
    lv_label_set_text(statusLabel, "❌ DH derive");
    return;
  }

  Serial.println("✅ Shared secret OK");
  lv_label_set_text(statusLabel, "✅ Secure!");
  hasHandledResponse = true;


  // ✅ Only mark as no longer waiting after successful parse
  hasHandledResponse = true;
  waitingResponse = false;
}



// === Download config ===
void onDownloadPressed(lv_event_t* e) {
  wifi.connect(10000);
  if (!wifi.isConnected()) {
    lv_label_set_text(statusLabel, "WiFi fail");
    return;
  }

  HTTPClient http;
  http.begin("http://192.168.1.141:5555/download/watch1");
  int code = http.GET();
  if (code == 200) {
    String json = http.getString();
    config = new CommanderConfigModule(json);
    if (!crypto.loadFromConfig(*config)) {
      lv_label_set_text(statusLabel, "❌ Crypto error");
      return;
    }
    lv_label_set_text(statusLabel, "✅ Config OK");
    lv_obj_clear_flag(startMissionBtn, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_label_set_text(statusLabel, "❌ Download error");
  }
  http.end();
  wifi.disconnect();
}

// === UI ===
void setupUI() {
  statusLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(statusLabel, "Commander Ready");
  lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 20);

  downloadBtn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(downloadBtn, 160, 50);
  lv_obj_align(downloadBtn, LV_ALIGN_CENTER, 0, -40);
  lv_obj_add_event_cb(downloadBtn, onDownloadPressed, LV_EVENT_CLICKED, NULL);
  lv_obj_t* label1 = lv_label_create(downloadBtn);
  lv_label_set_text(label1, "Download Config");
  lv_obj_center(label1);

  startMissionBtn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(startMissionBtn, 160, 50);
  lv_obj_align(startMissionBtn, LV_ALIGN_CENTER, 0, 40);
  lv_obj_add_event_cb(startMissionBtn, onStartPressed, LV_EVENT_CLICKED, NULL);
  lv_obj_t* label2 = lv_label_create(startMissionBtn);
  lv_label_set_text(label2, "Start Mission");
  lv_obj_center(label2);
  lv_obj_add_flag(startMissionBtn, LV_OBJ_FLAG_HIDDEN);
}

void setup() {
  Serial.begin(115200);
  watch.begin();
  beginLvglHelper();
  setupUI();
  lora.setup(false);
  
  lora.setOnReadData([](const uint8_t* pkt, size_t len) {
    lora.onLoraFileDataReceived(pkt, len);
});
lora.onFileReceived = onLoRaData;

}

void loop() {
  lv_timer_handler();
  delay(10);
  lora.readData();

  if (waitingResponse && millis() - startWaitTime > TIMEOUT_MS) {
    lv_label_set_text(statusLabel, "❌ Timeout");
    waitingResponse = false;
  }
}
