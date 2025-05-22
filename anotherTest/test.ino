// // *****************************************************************************************
// // Soldier ECDH Receiver - Handles certificate verification and ECDH secure key agreement
// // *****************************************************************************************

// #include <LilyGoLib.h>
// #include "../LoraModule/LoraModule.h"
// #include "../commander-config/commander-config.h"
// #include "crypto-helper.h"
// #include "ECDHHelper.h"
// #include <WiFiUdp.h> 
// #include "../wifi-connection/WifiModule.h"
// #include <LV_Helper.h>
// #include <ArduinoJson.h>
// #include <FFat.h>
// #include <HTTPClient.h>
// #include <vector>
// #include "mbedtls/base64.h"
// #include "../encryption/CryptoModule.h"
// #include <cstring>
// #include "../gps-collect/GPSModule.h"
// #include "SoldiersSentData.h"

// // === Module Instances ===
// LoraModule lora(433.5);                          // LoRa radio on 868 MHz
// CommanderConfigModule* config = nullptr;        // Configuration received from server
// certModule cryptohelper;                            // Handles cert + key parsing and verification
// ECDHHelper ecdh;                                // Handles ephemeral ECDH operations
// WifiModule wifi("default", "1357924680");         // Connects to web server to download config

// bool finished = false;

// std::unique_ptr<LoraModule> loraModule;
// std::unique_ptr<WifiModule> wifiModule;

// const char* ssid = "default";
// const char* password = "1357924680";

// const char *udpAddress = "192.168.0.44";
// const int udpPort = 3333;

// const crypto::Key256 SHARED_KEY = []() {
//   crypto::Key256 key{};
//   const char* raw = "0123456789abcdef0123456789abcdef"; 
//   std::memcpy(key.data(), raw, 32);
//   return key;
// }();

// SoldiersSentData coords[] = {
//   {0, 0, 31.970866, 34.785611, 78, 1},     // healthy
//   {0, 0, 31.970870, 34.785630, 100, 2},    // borderline
//   {0, 0, 31.970855, 34.785590, 55, 3},     // low
//   {0, 0, 31.970840, 34.785570, 0, 4},      // dead 
//   {0, 0, 31.970840, 34.785570, 0, 4},
//   {0, 0, 31.970840, 34.785570, 0, 4},
//   {0, 0, 31.970840, 34.785570, 0, 4},
//   {0, 0, 31.970840, 34.785570, 0, 4},
//   {0, 0, 31.970840, 34.785570, 0, 4},     // high
//   {0, 0, 31.970840, 34.785570, 0, 4},

// };

// const int coordCount = sizeof(coords) / sizeof(coords[0]);
// int currentIndex = 0;
// bool alreadyTouched = false;
// int transmissionState = RADIOLIB_ERR_NONE;
// lv_obj_t* sendLabel = NULL;
// lv_timer_t * sendTimer;
// WiFiUDP udp;
// std::unique_ptr<GPSModule> gpsModule;
// const char* helmentDownloadLink = "https://iconduck.com/api/v2/vectors/vctr0xb8urkk/media/png/256/download";

// volatile bool pmu_flag = false;
// volatile bool startGPSChecking = false;

// unsigned long lastSendTime = 0;
// const unsigned long SEND_INTERVAL = 5000;


// void IRAM_ATTR onPmuInterrupt() {
//   pmu_flag = true;
// }

// bool saveTileToFFat(const uint8_t* data, size_t len, const char* tileFilePath) {
//   File file = FFat.open(tileFilePath, FILE_WRITE);
//   if (!file) {
//       return false;
//   }
//   file.write(data, len);
//   file.close();
//   return true;
// }

// bool downloadFile(const std::string &tileURL, const char* fileName) {
//   HTTPClient http;
//   http.begin(tileURL.c_str());
//   http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
//                                "AppleWebKit/537.36 (KHTML, like Gecko) "
//                                "Chrome/134.0.0.0 Safari/537.36");
//   http.addHeader("Referer", "https://www.iconduck.com/");
//   http.addHeader("Accept", "image/png");

//   int httpCode = http.GET();
//   if (httpCode == HTTP_CODE_OK) {
//       size_t fileSize = http.getSize();
//       WiFiClient* stream = http.getStreamPtr();
//       uint8_t* buffer = new uint8_t[fileSize];
//       stream->readBytes(buffer, fileSize);
//       bool success = saveTileToFFat(buffer, fileSize, fileName);
//       delete[] buffer;
//       http.end();
//       return success;
//   } else {
//       http.end();
//       return false;
//   }
// }


// void showHelment() {
//   lv_obj_t * helmetIMG = lv_img_create(lv_scr_act());

//   lv_img_set_src(helmetIMG, "A:/helmet.png");
//   lv_obj_align(helmetIMG, LV_ALIGN_BOTTOM_MID, 0, 70);

//   lv_img_set_zoom(helmetIMG, 64);

//   Serial.println("Showing middle tile");
// }

// // === Setup ===
// void soldierSetup() {
//   Serial.println("Touch screen to send next coordinate.");
//   if(!FFat.exists("/helmet.png"))
//   {
//     downloadFile(helmentDownloadLink, "/helmet.png");
//   }


//   loraModule = std::make_unique<LoraModule>(433.5);
//   loraModule->setup(true);

//   wifiModule = std::make_unique<WifiModule>(ssid, password);
//   wifiModule->connect(60000);
//   Serial.printf("WiFi connected!");

//   struct tm timeInfo;
//   Serial.println("HELLO WORLD!");
//   setenv("TZ", "GMT-3", 1);
//   tzset();
//   deleteExistingFile("/cert.pem");
// // Create cert.pem file with certificate content if it does not exist
//   if (!FFat.exists("/cert.pem")) {
//     Serial.println("cert file not exists!");
//     File certFile = FFat.open("/cert.pem", FILE_WRITE);
//     if (certFile) {
//       Serial.println("cert file opened!");
//       const char* certContent = R"(-----BEGIN CERTIFICATE-----
// MIID1zCCAb+gAwIBAgIHEOPaLFZerzANBgkqhkiG9w0BAQsFADBFMQswCQYDVQQG
// EwJJTDETMBEGA1UECAwKU29tZS1TdGF0ZTEhMB8GA1UECgwYSW50ZXJuZXQgV2lk
// Z2l0cyBQdHkgTHRkMB4XDTI1MDUwOTEwMjAzOVoXDTI2MDUwOTEwMjAzOVowEzER
// MA8GA1UEAxMIc29sZGllcjEwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIB
// AQC0oQS/0YaIPw8jJ0b3Y26H3Y0raWKB5LUqGz9fooIaMUTyD1ouJ0bKzh5vhPOo
// +igTlU+MF+USt4Ey2R3O15fwYKmlzpbPwEkDGEXsA+SJQlPooXHjxbgAsB87rPil
// fvCOdCkbPaNYl14sfbjGJFTfPCDPiPBGCJJ/YU9hFoQDRo0OetOZ8rkXhoyukywT
// u6UPaTgo0w/SRaw3PV2Ea3du/+XrcAYL5lcyhYXUbPUWU8vk2zCSlxkaKeW3bSSw
// 8Ms9A18/1teasRvWu5IEoIj8e258Ah5KAO2pxDEvCuC1Pv0+0fp3GhEGQ34Yjbd9
// DZ21B7jKLLcLqo1xmsa4pQ+BAgMBAAEwDQYJKoZIhvcNAQELBQADggIBAF9vpL3F
// cwwJz3vZTQkJjuAiY8HdiKJ3iRKC/y853W8ZpmCwLLphSC5VD7vMIt+VlYuf5gNM
// Ms6eohzXlAVeNsaQJ3KW+5PI6Q/9SmqcFes/qpwQ39/EvkLpPskPSEqqEWaoSOd0
// 60+KrRT5u4DG3dgWtWW5i53709dv2wSXDCK3PWzkptvxugA2+2wZwrdfnVeLH8fb
// 2bp3/61Bn7aavCLC/NadmqGaPWA3AuXJFll+u6sQD46WK4ChH94d5cx88pxji6sM
// s5Ki2DcCDrpG37YsXbF4/gUvJL5Z8vIn5swXZSOnZevBMDddLH8xT7RTE13q+ZYB
// Q+695pvpl33QXLSFhGRZPA3K2Kpn87pEPJNou0JkkGxP+jYk8uYpbWlB1tfnC3dt
// nR6dFo4iKY9WFc6LHDxtYzf+nThoz9o74NTpY790OUcxbt1d6oaK+15ob+RghSW6
// Fj2Jb91Vsm96HozE8B3Xn+XwBB2jzrAqYWs5VvFqsm2NchVy7zZ+L1Z4eh3DRqUQ
// ErXSoivNVFfTDhTKpuGQs4qF6NCYUWFn5LO4QyHuuorlyLZXhnsHOxlZIktVRgkr
// GLLseRj4Yp23+4z4o3hpEavecLgFIhuT9yBMBvxbDm9G3KLvbYYEaFOx1ItLLVnn
// WnuAIqZGA5XJrhEujtZqR7EecTPgYH2pD8d8
// -----END CERTIFICATE-----
// )";
//       certFile.print(certContent);
//       certFile.close();
//     }
//   }
//   else
//   {
//     Serial.println("Cert file exists!");
//   }

//   configTime(0, 0, "pool.ntp.org");
//   while (!getLocalTime(&timeInfo)) {
//       Serial.println("Waiting for time sync...");
//       delay(500);
//   }

//   if (getLocalTime(&timeInfo)) {
//       char timeStr[20];
//       strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);
//       Serial.println(timeStr);
//   }
//   crypto::CryptoModule::init();
//   listFiles("/");

//   watch.attachPMU(onPmuInterrupt);
//   watch.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
//   watch.clearPMU();
//   watch.enableIRQ(
//       XPOWERS_AXP2101_PKEY_SHORT_IRQ | 
//       XPOWERS_AXP2101_PKEY_LONG_IRQ
//   );


//   init_main_poc_page();
// }

// std::pair<float, float> getTileCenterLatLon(float lat, float lon, int zoomLevel, float tileSize) {

//   double lat_rad = lat * M_PI / 180.0;
//   double n = std::pow(2.0, zoomLevel);

//   int x_tile = static_cast<int>(std::floor((lon + 180.0) / 360.0 * n));
//   int y_tile = static_cast<int>(std::floor((1.0 - std::log(std::tan(lat_rad) + 1.0 / std::cos(lat_rad)) / M_PI) / 2.0 * n));

//   int centerX = x_tile * tileSize + tileSize / 2;
//   int centerY = y_tile * tileSize + tileSize / 2;

//   double lon_deg = centerX / (n * tileSize) * 360.0 - 180.0;
//   double lat_rad_center = std::atan(std::sinh(M_PI * (1 - 2.0 * centerY / (n * tileSize))));
//   double lat_deg = lat_rad_center * 180.0 / M_PI;

//   return {static_cast<float>(lat_deg), static_cast<float>(lon_deg)};
// }

// void clearMainPage()
// {
//     lv_obj_t * mainPage = lv_scr_act();
//     lv_obj_remove_style_all(mainPage);
//     lv_obj_clean(mainPage);

//     sendLabel = NULL;

// }

// void deleteExistingFile(const char* tileFilePath) {
//   if (FFat.exists(tileFilePath)) {
//       Serial.println("Deleting existing tile...");
//       FFat.remove(tileFilePath);
//   }
// }

// void listFiles(const char* dirname) {

//   File root = FFat.open(dirname);
//   if (!root) {
//       return;
//   }
//   if (!root.isDirectory()) {
//       return;
//   }

//   File file = root.openNextFile();
//   while (file) {
//       if (file.isDirectory()) {
//       } else {
//       }
//       file = root.openNextFile();
//   }
// }

// void init_send_large_log_file_page(lv_event_t * event)
// {
//   Serial.println("init_send_large_log_file_page");
//   File file = FFat.open("/cert.pem", FILE_READ);
//   if (file)
//   {
//     size_t size = file.size();
//     uint8_t* buff = static_cast<uint8_t*>(malloc(size));
//     Serial.printf("%d %d", size, sizeof(buff));
//     if (!buff)
//     {
//       Serial.println("NO BUFF FOUND");
//       return;
//     }

//     size_t n = file.read(buff, size);
//     file.close();
//     Serial.printf("BUFF IS: %s\n", (char*)buff);
//     loraModule->sendFile(buff, n);

//   }
//   else
//   {
//     Serial.println("NO FILE FOUND");
//   }
// }

// void init_main_poc_page()
// {
//     Serial.println("init_main_poc_page");
//     clearMainPage();

//     lv_obj_t * mainPage = lv_scr_act();
//     lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
//     lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

//     showHelment();

//     lv_obj_t *mainLabel = lv_label_create(mainPage);
//     lv_label_set_text(mainLabel, "TactiGrid Soldier");
//     lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
//     lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

//     lv_obj_t *sendCoordsBtn = lv_btn_create(mainPage);
//     lv_obj_center(sendCoordsBtn);
//     lv_obj_set_style_bg_color(sendCoordsBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
//     lv_obj_add_event_cb(sendCoordsBtn, init_send_coords_page, LV_EVENT_CLICKED, NULL);

//     lv_obj_t *sendCoordsLabel = lv_label_create(sendCoordsBtn);
//     lv_label_set_text(sendCoordsLabel, "Send Coords");
//     lv_obj_center(sendCoordsLabel);

//     // lv_obj_t *sendLogFileBtn = lv_btn_create(mainPage);
//     // lv_obj_align(sendLogFileBtn, LV_ALIGN_CENTER, 80, 0);
//     // lv_obj_set_style_bg_color(sendLogFileBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
//     // lv_obj_add_event_cb(sendLogFileBtn, init_send_large_log_file_page, LV_EVENT_CLICKED, NULL);

//     // lv_obj_t *sendLogFileLabel = lv_label_create(sendLogFileBtn);
//     // lv_label_set_text(sendLogFileLabel, "Send Log File");
//     // lv_obj_center(sendLogFileLabel);

//     startGPSChecking = false;
// }


// void init_send_coords_page(lv_event_t * event)
// {
//   Serial.println("init_send_coords_page");
//   clearMainPage();

//   lv_obj_t * mainPage = lv_scr_act();
//   lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
//   lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

//   lv_obj_t *mainLabel = lv_label_create(mainPage);
//   lv_label_set_text(mainLabel, "TactiGrid Soldier");
//   lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
//   lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);


//   sendLabel = lv_label_create(mainPage);
//   lv_obj_align(sendLabel, LV_ALIGN_TOP_MID, 0, 15);
//   lv_obj_set_style_text_color(sendLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
//   lv_label_set_text(sendLabel, "");

//   lv_obj_set_width(sendLabel, lv_disp_get_hor_res(NULL) - 20);
//   lv_label_set_long_mode(sendLabel, LV_LABEL_LONG_WRAP);

//   sendTimer = lv_timer_create(sendTimerCallback, 5000, NULL);
//   //startGPSChecking = true;

// }

// void sendTimerCallback(lv_timer_t *timer) {
//   if(currentIndex < 9) {

//     struct tm timeInfo;
//     char timeStr[9];

//     if(getLocalTime(&timeInfo)) {
//         strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeInfo);
//     } else {
//         strcpy(timeStr, "00:00:00");
//     }
    
//     sendCoordinate(coords[currentIndex % coordCount].posLat, coords[currentIndex % coordCount].posLon, coords[currentIndex % coordCount].heartRate);
    
//     const char *current_text = lv_label_get_text(sendLabel);
    
//     lv_label_set_text_fmt(sendLabel, "%s%s - sent coords {%.5f, %.5f}\n", 
//                           current_text, timeStr, 
//                           coords[currentIndex % coordCount].posLat, 
//                           coords[currentIndex % coordCount].posLon);
                          
//     currentIndex++;
//   } else {
//       lv_timer_del(timer);
//       currentIndex = 0;
//   }
// }

// bool isZero(float x)
// {
//     return std::fabs(x) < 1e-9f;
// }


// void sendCoordinate(float lat, float lon, int heartRate) {
//   Serial.println("sendCoordinate");

//   SoldiersSentData coord;
//   coord.posLat = lat;
//   coord.posLon = lon;
//   coord.heartRate = heartRate;

//   Serial.printf("BEFORE SENDING: %.7f %.7f %.7f %.7f %d\n",coord.tileLat, coord.tileLon, coord.posLat, coord.posLon, coord.heartRate);
//   auto [tileLat, tileLon] = getTileCenterLatLon(coord.posLat, coord.posLon, 19, 256);

//   coord.tileLat = tileLat;
//   coord.tileLon = tileLon;
//   Serial.printf("SENDING: %.7f %.7f %.7f %.7f %d\n",coord.tileLat, coord.tileLon, coord.posLat, coord.posLon, coord.heartRate);
//   crypto::ByteVec payload;
//   payload.resize(sizeof(SoldiersSentData));
//   std::memcpy(payload.data(), &coord, sizeof(SoldiersSentData));
  
//   crypto::Ciphertext ct = crypto::CryptoModule::encrypt(SHARED_KEY, payload);

//   auto appendHex = [](String& s, uint8_t b) {
//     if (b < 0x10) s += "0";
//     s += String(b, HEX);
//   };

//   String msg;
//   for (auto b : ct.nonce) appendHex(msg, b);
//   msg += "|";
//   for (auto b : ct.data)  appendHex(msg, b);
//   msg += "|";
//   for (auto b : ct.tag)   appendHex(msg, b);

//   udp.beginPacket(udpAddress, udpPort);
//   udp.printf(msg.c_str());
//   udp.endPacket();
//   transmissionState = loraModule->sendData(msg.c_str());

//   struct tm timeInfo;
//   char timeStr[9];

//   if(getLocalTime(&timeInfo)) {
//       strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeInfo);
//   } else {
//       strcpy(timeStr, "00:00:00");
//   }
        
//   lv_label_set_text_fmt(sendLabel, "%s - sent coords {%.5f, %.5f}\n", 
//                         timeStr,
//                         lat, 
//                         lon);
// }


// // === UI Elements ===
// lv_obj_t* statusLabel;
// lv_obj_t* downloadBtn;
// lv_obj_t* startMissionBtn;

// bool hasResponded = false;


// // === Commander ECDH data reception ===
// String incomingMessage;
// bool receiving = false;
// unsigned long lastReceiveTime = 0;
// const unsigned long TIMEOUT_MS = 10000;

// // === Helper: base64 encode byte vector ===
// String toBase64(const std::vector<uint8_t>& input) {
//   size_t outLen = 4 * ((input.size() + 2) / 3);
//   std::vector<uint8_t> outBuf(outLen + 1); // +1 for null terminator
//   size_t actualLen = 0;

//   int ret = mbedtls_base64_encode(outBuf.data(), outBuf.size(), &actualLen,
//                                    input.data(), input.size());

//   if (ret != 0) {
//     Serial.println("❌ Base64 encode failed");
//     return "";
//   }

//   outBuf[actualLen] = '\0';
//   return String((const char*)outBuf.data());
// }

// // === Helper: Decode base64 to byte vector ===
// bool decodeBase64(const String& input, std::vector<uint8_t>& output) {
//   size_t inputLen = input.length();
//   size_t decodedLen = (inputLen * 3) / 4 + 2;
//   output.resize(decodedLen);
//   size_t outLen = 0;
//   int ret = mbedtls_base64_decode(output.data(), output.size(), &outLen,
//                                    (const uint8_t*)input.c_str(), inputLen);
//   if (ret != 0) return false;
//   output.resize(outLen);
//   return true;
// }

// void sendECDHResponse(int toId) {
//   Serial.println("📤 Sending soldier response...");

//   // ✅ Step 1: Get the PEM string and fix line endings
//   String certB64 = config->getCertificatePEM();
// String ephB64 = toBase64(ecdh.getPublicKeyRaw());

//   DynamicJsonDocument doc(4096);
//   doc["id"] = config->getId();
//   doc["cert"] = certB64;
//   doc["ephemeral"] = ephB64;


//   // ✅ Step 6: Serialize and send
//   String out;
//   serializeJsonPretty(doc, out);
//   lora.sendFile((const uint8_t*)out.c_str(), out.length(), 180);
//   lv_label_set_text(statusLabel, "✅ Response sent");

// }




// // === Handle incoming commander ECDH message ===
// void onLoRaData(const uint8_t* data, size_t len) {
//   if (hasResponded) {
//     Serial.println("⚠️ Already responded, ignoring message");
//     return;
//   }   

//   receiving = false;
//   String msg((const char*)data, len);
//   Serial.println("📥 Received commander message:");
//   Serial.println(msg);

//   DynamicJsonDocument doc(4096);
//   if (deserializeJson(doc, msg) != DeserializationError::Ok) {
//     lv_label_set_text(statusLabel, "❌ JSON error");
//     return;
//   }

//   int recipientId = doc["id"];
//   if (recipientId != config->getId()) {
//     lv_label_set_text(statusLabel, "⚠️ Not for me");
//     return;
//   }

//   // === Step 1: Verify commander's certificate ===
//   String certB64 = doc["cert"].as<String>();
//   std::vector<uint8_t> certRaw;
//   if (!decodeBase64(certB64, certRaw)) {
//     lv_label_set_text(statusLabel, "❌ Cert decode");
//     return;
//   }

//   if (!cryptohelper.loadSingleCertificate(String((const char*)certRaw.data()))) {
//     lv_label_set_text(statusLabel, "❌ Cert parse");
//     return;
//   }

//   if (!cryptohelper.verifyCertificate()) {
//     lv_label_set_text(statusLabel, "❌ Invalid cert");
//     return;
//   }

//   Serial.println("✅ Commander cert valid");

//   // === Step 2: Load and import commander's ephemeral key ===
//   String ephB64 = doc["ephemeral"].as<String>();
//   std::vector<uint8_t> ephRaw;
//   if (!decodeBase64(ephB64, ephRaw)) {
//     lv_label_set_text(statusLabel, "❌ E key decode");
//     return;
//   }

//   if (!ecdh.importPeerPublicKey(ephRaw)) {
//     lv_label_set_text(statusLabel, "❌ E key import");
//     return;
//   }

//   // === Step 3: Generate own ephemeral key and compute shared secret ===
//   if (!ecdh.generateEphemeralKey()) {
//     lv_label_set_text(statusLabel, "❌ My keygen");
//     return;
//   }

//   std::vector<uint8_t> shared;
//   if (!ecdh.deriveSharedSecret(shared)) {
//     lv_label_set_text(statusLabel, "❌ DH derive");
//     return;
//   }

//   Serial.println("✅ Shared secret OK");
//   lv_label_set_text(statusLabel, "✅ Secure!");

// //✅ Mark as responded
//   hasResponded = true;

//   // === Step 4: Respond back with our cert + ephemeral ===
//   sendECDHResponse(doc["id"]);
//   finished = true;

//   Serial.printf("Received GMK: %s\n", "0123456789abcdef0123456789abcdef");
//   clearMainPage();
//   soldierSetup();

// }



// // === Download config from Wi-Fi server ===
// void onDownloadPressed(lv_event_t* e) {
//   wifi.connect(10000);
//   if (!wifi.isConnected()) {
//     lv_label_set_text(statusLabel, "WiFi fail");
//     return;
//   }

//   HTTPClient http;
//   http.begin("http://192.168.0.44:5555/download/watch2");
//   int httpCode = http.GET();
//   if (httpCode == 200) {
//     String payload = http.getString();
//     config = new CommanderConfigModule(payload);
//     if (!cryptohelper.loadFromConfig(*config)) {
//       lv_label_set_text(statusLabel, "Crypto error");
//       return;
//     }
//     lv_label_set_text(statusLabel, "Download OK");
//     lv_obj_clear_flag(startMissionBtn, LV_OBJ_FLAG_HIDDEN);
//   } else {
//     lv_label_set_text(statusLabel, "HTTP fail");
//   }
//   http.end();
//   wifi.disconnect();
// }

// // === Start listening for commander ECDH init ===
// void onStartMissionPressed(lv_event_t* e) {
//   lv_label_set_text(statusLabel, "Waiting commander...");
//   receiving = true;
//   lastReceiveTime = millis();
//   lora.setupListening();
  
//   lora.setOnReadData([](const uint8_t* pkt, size_t len) {
//     lora.onLoraFileDataReceived(pkt, len);  // ✅ handles reassembly
// });
// lora.onFileReceived = onLoRaData;  // ✅ called only when full file is ready

// }

// // === UI Setup ===
// void setupUI() {
//   statusLabel = lv_label_create(lv_scr_act());
//   lv_label_set_text(statusLabel, "Init soldier...");
//   lv_obj_align(statusLabel, LV_ALIGN_TOP_MID, 0, 20);

//   downloadBtn = lv_btn_create(lv_scr_act());
//   lv_obj_set_size(downloadBtn, 160, 50);
//   lv_obj_align(downloadBtn, LV_ALIGN_CENTER, 0, -40);
//   lv_obj_add_event_cb(downloadBtn, onDownloadPressed, LV_EVENT_CLICKED, NULL);
//   lv_obj_t* label1 = lv_label_create(downloadBtn);
//   lv_label_set_text(label1, "Download Config");
//   lv_obj_center(label1);

//   startMissionBtn = lv_btn_create(lv_scr_act());
//   lv_obj_set_size(startMissionBtn, 160, 50);
//   lv_obj_align(startMissionBtn, LV_ALIGN_CENTER, 0, 40);
//   lv_obj_add_event_cb(startMissionBtn, onStartMissionPressed, LV_EVENT_CLICKED, NULL);
//   lv_obj_t* label2 = lv_label_create(startMissionBtn);
//   lv_label_set_text(label2, "Wait for Commander");
//   lv_obj_center(label2);
//   lv_obj_add_flag(startMissionBtn, LV_OBJ_FLAG_HIDDEN);
// }

// // === Main setup ===
// void setup() {
//   Serial.begin(115200);
//   watch.begin();
//   beginLvglHelper();
//   setupUI();
//   lora.setup(false);  // Receiver mode
//   lora.setupListening();

//   lora.setOnReadData([](const uint8_t* pkt, size_t len) {
//     lora.onLoraFileDataReceived(pkt, len);
//   }

//   );


//   Serial.println("finish init message from commander");
//   lora.onFileReceived = onLoRaData;  // <-- THIS must call your JSON handler
//   gpsModule = std::make_unique<GPSModule>();
// }


// // === Main loop ===
// void loop() {
//   lv_timer_handler();
//   delay(10);
//   if (!finished)
//   {
//     lora.readData();

//     if (receiving && millis() - lastReceiveTime > TIMEOUT_MS) {
//       lv_label_set_text(statusLabel, "❌ Timeout");
//       receiving = false;
//     }
//   }
//   else
//   {
//     if (pmu_flag) {
//       pmu_flag = false;
//       uint32_t status = watch.readPMU();
//       if (watch.isPekeyShortPressIrq()) {
//           init_main_poc_page();
//       }
//       watch.clearPMU();
//     }

//     loraModule->cleanUpTransmissions();

//     if (startGPSChecking) {
//       gpsModule->readGPSData();

//       gpsModule->updateCoords();
//       unsigned long currentTime = millis();

//       float lat = gpsModule->getLat();
//       float lon = gpsModule->getLon();

//       if (currentTime - lastSendTime >= SEND_INTERVAL) {

//         if (!isZero(lat) && !isZero(lon)) {
//           sendCoordinate(lat, lon, 100);
//         }
//         else
//         {
//           TinyGPSSpeed gpsSpeed = gpsModule->getGPSSpeed();
//           TinyGPSInteger gpsSatellites = gpsModule->getGPSSatellites();
//           TinyGPSAltitude gpsAltitude = gpsModule->getGPSAltitude();
//           TinyGPSHDOP gpsHDOP = gpsModule->getGPSHDOP();
//           TinyGPSTime gpsTime = gpsModule->getGPSTime();
//           TinyGPSDate gpsDate = gpsModule->getGPSDate();

//           uint32_t  satellites = gpsSatellites.isValid() ? gpsSatellites.value() : 0;
//           double hdop = gpsHDOP.isValid() ? gpsHDOP.hdop() : 0;
//           uint16_t year = gpsDate.isValid() ? gpsDate.year() : 0;
//           uint8_t  month = gpsDate.isValid() ? gpsDate.month() : 0;
//           uint8_t  day = gpsDate.isValid() ? gpsDate.day() : 0;
//           uint8_t  hour = gpsTime.isValid() ? gpsTime.hour() : 0;
//           uint8_t  minute = gpsTime.isValid() ? gpsTime.minute() : 0;
//           uint8_t  second = gpsTime.isValid() ? gpsTime.second() : 0;
//           double  meters = gpsAltitude.isValid() ? gpsAltitude.meters() : 0;
//           double  kmph = gpsSpeed.isValid() ? gpsSpeed.kmph() : 0;
//           lv_label_set_text_fmt(sendLabel, "Sats:%u\nHDOP:%.1f\nLat:%.5f\nLon:%.5f\nDate:%d/%d/%d \nTime:%d/%d/%d\nAlt:%.2f m \nSpeed:%.2f",
//             satellites, hdop, lat, lon, year, month, day, hour, minute, second, meters, kmph);
//         }

//         lastSendTime = currentTime;
//       }

//     }
//   }
// }















// ***************************************************************************************
// Commander ECDH Initiator - Sends certificate + ephemeral key, verifies soldier reply
// ***************************************************************************************

#define LV_HOR_RES_MAX 240
#define LV_VER_RES_MAX 240


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
#include "../encryption/CryptoModule.h"
#include "../envLoader.cpp"
#include "../soldier/SoldiersSentData.h"
#include <memory>
#include <FFat.h>
#include <cstring>


const crypto::Key256 SHARED_KEY = []() {
    crypto::Key256 key{};

    const char* raw = "0123456789abcdef0123456789abcdef"; 
    std::memcpy(key.data(), raw, 32);
    return key;
}();

// === Modules ===
LoraModule lora(433.5);
CommanderConfigModule* config = nullptr;
certModule cryptohelper;
ECDHHelper ecdh;
WifiModule wifi("default", "1357924680");

// === UI ===
lv_obj_t* statusLabel;
lv_obj_t* downloadBtn;
lv_obj_t* startMissionBtn;

// === additional ===
bool hasHandledResponse = false;
bool finished = false;
volatile bool pmu_flag = false;
// === Soldier response ===
bool waitingResponse = false;
unsigned long startWaitTime = 0;
const unsigned long TIMEOUT_MS = 15000;

lv_obj_t *current_marker = NULL;

std::unique_ptr<LoraModule> loraModule;
std::unique_ptr<WifiModule> wifiModule;

const std::string tileUrlInitial = "https://tile.openstreetmap.org/";
const char* tileFilePath = "/middleTile.png";
const char* logFilePath = "/log.txt";

using GPSCoordTuple = std::tuple<float, float, float, float>;

lv_obj_t* soldiersNameLabel = NULL;

lv_color_t getColorFromHeartRate(int hr) {
    if (hr <= 0) return lv_color_black();

    const int min_hr = 40;
    const int max_hr = 140;

    if (hr < min_hr)
    {
        hr = min_hr;
    } 
    else if (hr > max_hr)
    {
        hr = max_hr;
    } 

    int hue = 120 * (hr - min_hr) / (max_hr - min_hr);

    return lv_color_hsv_to_rgb(hue, 100, 100);
}

static inline crypto::ByteVec hexToBytes(const String& hex) {
    crypto::ByteVec out;
    out.reserve(hex.length() >> 1);
    for (int i = 0; i < hex.length(); i += 2) {
        out.push_back(strtoul(hex.substring(i, i + 2).c_str(), nullptr, 16));
    }
    return out;
}

lv_color_t ballColor = lv_color_hex(0xff0000);

void IRAM_ATTR onPmuInterrupt() {
    pmu_flag = true;
}

void anim_opacity_cb(void * obj, int32_t value) {
    lv_obj_set_style_opa((lv_obj_t *)obj, value, 0);
}

std::tuple<int, int, int> positionToTile(float lat, float lon, int zoom)
{
    float lat_rad = radians(lat);
    float n = pow(2.0, zoom);
    
    int x_tile = floor((lon + 180.0) / 360.0 * n);
    int y_tile = floor((1.0 - log(tan(lat_rad) + 1.0 / cos(lat_rad)) / PI) / 2.0 * n);
    
    return std::make_tuple(zoom, x_tile, y_tile);
}

std::tuple<int,int> latlon_to_pixel(double lat, double lon, double centerLat, double centerLon, int zoom)
{
    static constexpr double TILE_SIZE = 256.0;

    auto toGlobal = [&](double la, double lo) {
        double rad = la * M_PI / 180.0;
        double n   = std::pow(2.0, zoom);
        int xg = (int) std::floor((lo + 180.0) / 360.0 * n * TILE_SIZE);
        int yg = (int) std::floor((1.0 - std::log(std::tan(rad) + 1.0 / std::cos(rad)) / M_PI) / 2.0 * n * TILE_SIZE);
        return std::make_pair(xg, yg);
    };

    auto [markerX, markerY] = toGlobal(lat, lon);
    auto [centerX, centerY] = toGlobal(centerLat, centerLon);

    int localX = markerX - centerX + (LV_HOR_RES_MAX / 2);
    int localY = markerY - centerY + (LV_VER_RES_MAX / 2);

    return std::make_tuple(localX, localY);
}

std::pair<double,double> tileCenterLatLon(int zoom, int x_tile, int y_tile)
{
    static constexpr double TILE_SIZE = 256.0;
    // Compute the center pixel of the tile
    int centerX = x_tile * TILE_SIZE + TILE_SIZE / 2;
    int centerY = y_tile * TILE_SIZE + TILE_SIZE / 2;
    
    double n = std::pow(2.0, zoom);
    double lon_deg = (double)centerX / (n * TILE_SIZE) * 360.0 - 180.0;
    double lat_rad = std::atan(std::sinh(M_PI * (1 - 2.0 * centerY / (n * TILE_SIZE))));
    double lat_deg = lat_rad * 180.0 / M_PI;
    
    return {lat_deg, lon_deg};
}

void create_fading_circle(double markerLat, double markerLon, double centerLat, double centerLon, int zoom, lv_color_t ballColor) {
    if (current_marker != NULL) {
        lv_obj_del(current_marker);
        current_marker = NULL;
    }

    int pixel_x, pixel_y;
    std::tie(pixel_x, pixel_y) = latlon_to_pixel(markerLat, markerLon, centerLat, centerLon, zoom);

    current_marker = lv_obj_create(lv_scr_act());
    lv_obj_set_size(current_marker, 10, 10);
    lv_obj_set_style_bg_color(current_marker, ballColor, 0);
    lv_obj_set_style_radius(current_marker, LV_RADIUS_CIRCLE, 0);

    lv_obj_set_pos(current_marker, pixel_x, pixel_y);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, current_marker);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_time(&a, 1000);
    lv_anim_set_playback_time(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_exec_cb(&a, anim_opacity_cb);
    lv_anim_start(&a);

    if(!soldiersNameLabel)
    {
        soldiersNameLabel = lv_label_create(lv_scr_act());
        lv_label_set_text(soldiersNameLabel, "Soldier 1");
    }
    
    
    lv_obj_align_to(soldiersNameLabel, current_marker, LV_ALIGN_TOP_MID, 0, -35);
}

void clearMainPage()
{
    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_remove_style_all(mainPage);
    lv_obj_clean(mainPage);

    current_marker = NULL;
    soldiersNameLabel = NULL;

    deleteExistingFile(tileFilePath);
}

bool saveTileToFFat(const uint8_t* data, size_t len, const char* tileFilePath) {
    File file = FFat.open(tileFilePath, FILE_WRITE);
    if (!file) {
        return false;
    }
    file.write(data, len);
    file.close();
    return true;
}

bool downloadSingleTile(const std::string &tileURL, const char* fileName) {
    HTTPClient http;
    http.begin(tileURL.c_str());
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                 "AppleWebKit/537.36 (KHTML, like Gecko) "
                                 "Chrome/134.0.0.0 Safari/537.36");
    http.addHeader("Referer", "https://www.openstreetmap.org/");
    http.addHeader("Accept", "image/png");

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        ballColor = lv_color_hex(0x000000);
        size_t fileSize = http.getSize();
        WiFiClient* stream = http.getStreamPtr();
        uint8_t* buffer = new uint8_t[fileSize];
        stream->readBytes(buffer, fileSize);
        bool success = saveTileToFFat(buffer, fileSize, fileName);
        delete[] buffer;
        http.end();
        return success;
    } else {
        http.end();
        return false;
    }
}

bool downloadMiddleTile(const std::tuple<int, int, int> &tileLocation)
{
    std::string middleTile = tileUrlInitial +
        std::to_string(std::get<0>(tileLocation)) + "/" +
        std::to_string(std::get<1>(tileLocation)) + "/" +
        std::to_string(std::get<2>(tileLocation)) + ".png";
    return downloadSingleTile(middleTile, tileFilePath);
}

void showMiddleTile() {
    lv_obj_t * img1 = lv_img_create(lv_scr_act());
    lv_obj_center(img1);

    lv_img_set_src(img1, "A:/middleTile.png");
    lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);

    // Serial.println("Showing middle tile");
}


void deleteExistingFile(const char* tileFilePath) {
    if (FFat.exists(tileFilePath)) {
        // Serial.println("Deleting existing tile...");
        FFat.remove(tileFilePath);
    }
}


void init_upload_log_page(lv_event_t * event)
{
    clearMainPage();
    // Serial.println("Content:" + loadFileContent(logFilePath));
    lv_obj_t * mainPage = lv_scr_act();

    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "Log File Content");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *logContentLabel = lv_label_create(mainPage);
    lv_label_set_text(logContentLabel, loadFileContent(logFilePath).c_str());
    lv_obj_align(logContentLabel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(logContentLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    
    lv_obj_set_width(logContentLabel, lv_disp_get_hor_res(NULL) - 20);
    lv_label_set_long_mode(logContentLabel, LV_LABEL_LONG_WRAP);

    lv_obj_t *uploadBtn = lv_btn_create(mainPage);
    lv_obj_align(uploadBtn, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(uploadBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(uploadBtn, upload_log_event_callback, LV_EVENT_CLICKED, NULL);


    lv_obj_t *uploadBtnLabel = lv_label_create(uploadBtn);
    lv_label_set_text(uploadBtnLabel, "Upload Log");
    lv_obj_center(uploadBtnLabel);

}

void upload_log_event_callback(lv_event_t * e)
{
    // Serial.println("Starting sending log file");
    String logContent = loadFileContent(logFilePath);

    wifiModule->sendString(logContent.c_str(), "192.168.0.44", 5555);

    // Serial.println("File sent over UDP.");

    init_main_menu();
    //create_popup(lv_scr_act());

}

void msgbox_event_handler(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * mbox = lv_event_get_target(e);
        const char * btn_txt = lv_msgbox_get_active_btn_text(mbox);
        if(strcmp(btn_txt, "OK") == 0) {
            lv_obj_del_async(mbox);
            init_main_menu();
        }
    }
}

void create_popup(lv_obj_t * parent) {
    static const char * btns[] = {"OK", ""};
    
    lv_obj_t * mbox = lv_msgbox_create(parent, "Info", "Uploaded using UDP, port 5555!", btns, true);
    lv_obj_center(mbox);
    
    lv_obj_add_event_cb(mbox, msgbox_event_handler, LV_EVENT_VALUE_CHANGED, NULL);
}

String loadFileContent(const char* filePath)
{
    File file = FFat.open(filePath, FILE_READ);
    if (!file) {
        // Serial.println("Failed to open file for reading!");
        return "";
    }
    
    String fileContent;
    while (file.available()) {
        fileContent += (char)file.read();
    }

    file.close();

    return fileContent;
}

void listFiles(const char* path = "/", uint8_t depth = 0) {
    File dir = FFat.open(path);
    if (!dir || !dir.isDirectory()) {
        // Serial.printf("Failed to open directory: %s\n", path);
        return;
    }

    File file = dir.openNextFile();
    while (file) {
        for (uint8_t i = 0; i < depth; i++) Serial.print("  ");

        if (file.isDirectory()) {
            // Serial.printf("[DIR]  %s\n", file.name());

            listFiles(file.name(), depth + 1);
        } else {
            // Serial.printf("FILE:  %s  SIZE: %d\n", file.name(), file.size());
        }

        file = dir.openNextFile();
    }
}




void init_main_poc_page(lv_event_t * event)
{
    // Serial.println("init_main_poc_page");
    clearMainPage();

    deleteExistingFile(logFilePath);

    ballColor = lv_color_hex(0xff0000);
    lv_obj_t * mainPage = lv_scr_act();

    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "TactiGrid Commander");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* waitingLabel = lv_label_create(mainPage);
    lv_obj_center(waitingLabel);
    lv_label_set_text(waitingLabel, "Waiting For Initial Coords");
    lv_obj_set_style_text_color(waitingLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);


    loraModule->setupListening();
}

GPSCoordTuple parseCoordinates(const String &message) {
    float lat1 = 0, lon1 = 0, lat2 = 0, lon2 = 0;
    sscanf(message.c_str(), "%f,%f;%f,%f", &lat1, &lon1, &lat2, &lon2);
    return std::make_tuple(lat1, lon1, lat2, lon2);
}

bool isZero(double x)
{
    return std::fabs(x) < 1e-9;
}

void init_p2p_test(const uint8_t* data, size_t len)
{
    String incoming;
    for (size_t i = 0; i < len; i++) {
        incoming += (char)data[i];
    }

    int p1 = incoming.indexOf('|');
    int p2 = incoming.indexOf('|', p1 + 1);
    if (p1 < 0 || p2 < 0) {                      
        // Serial.println("Bad ciphertext format");
        return;
    }

    crypto::Ciphertext ct;
    ct.nonce = hexToBytes(incoming.substring(0, p1));
    ct.data  = hexToBytes(incoming.substring(p1 + 1, p2));
    ct.tag   = hexToBytes(incoming.substring(p2 + 1));

    // Serial.printf("Nonce size: %d\n", ct.nonce.size());
    // Serial.printf("Data size:  %d\n", ct.data.size());
    // Serial.printf("Tag size:   %d\n", ct.tag.size());
    
    crypto::ByteVec pt;

    try {
        pt = crypto::CryptoModule::decrypt(SHARED_KEY, ct);
    } catch (const std::exception& e) {
        // Serial.printf("Decryption failed: %s\n", e.what());
        return;
    }


    SoldiersSentData* newG = reinterpret_cast<SoldiersSentData*>(pt.data());
    // Serial.printf("%.5f %.5f %.5f %.5f %d\n", newG->lat1, newG->lat2, newG->lon1, newG->lon2, newG->heartRate);
    String plainStr;
    plainStr.reserve(pt.size());
    for (unsigned char b : pt) plainStr += (char)b;

    // Serial.println("Decrypted: " + plainStr);

    GPSCoordTuple coords = parseCoordinates(plainStr);

    float tile_lat = newG->tileLat;
    float tile_lon = newG->tileLon;
    float marker_lat = newG->posLat;
    float marker_lon = newG->posLon;

    if(isZero(tile_lat) || isZero(tile_lon) || isZero(marker_lat) || isZero(marker_lon))
    {
        return;
    }
    
    struct tm timeInfo;
    char timeStr[9];
    if (getLocalTime(&timeInfo))
        strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeInfo);
    else
        strcpy(timeStr, "00:00:00");

    char result[128];
    snprintf(result, sizeof(result), "%s - Soldier 1: {%.5f, %.5f}", 
             timeStr, tile_lat, tile_lon);
    writeToLogFile(logFilePath, result);

    std::tuple<int,int,int> tileLocation = positionToTile(tile_lat, tile_lon, 19);

    if (!FFat.exists(tileFilePath) && downloadMiddleTile(tileLocation))
        // Serial.println("Middle tile downloaded.");

    if (!current_marker)
        showMiddleTile();

    auto [zoom, x_tile, y_tile] = tileLocation;
    auto [centerLat, centerLon] = tileCenterLatLon(zoom, x_tile, y_tile);

    int heartRate = newG->heartRate;
    ballColor = getColorFromHeartRate(heartRate);
    Serial.printf("HEART RATE: %d\n", heartRate);
    create_fading_circle(marker_lat, marker_lon, centerLat, centerLon, 19, ballColor);

}

void init_receive_logs_page(lv_event_t * event)
{
    Serial.println("loraModule->setupListening()");
}

void init_main_menu()
{   
    clearMainPage();

    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);


    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "Main Menu");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *receiveFile = lv_btn_create(mainPage);
    lv_obj_set_size(receiveFile, 240, 60);
    lv_obj_set_style_bg_color(receiveFile, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(receiveFile, init_receive_logs_page, LV_EVENT_CLICKED, NULL);

    lv_obj_t *receiveFileLabel = lv_label_create(receiveFile);
    lv_label_set_text(receiveFileLabel, "Receive Logs");
    lv_obj_center(receiveFileLabel);

    lv_obj_t *cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cont, 240, 60);
    lv_obj_center(cont);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_flex_flow(cont, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(cont, lv_disp_get_hor_res(NULL), 60);

    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);

    lv_obj_t *receiveCoordsBtn = lv_btn_create(cont);
    lv_obj_center(receiveCoordsBtn);
    lv_obj_set_style_bg_color(receiveCoordsBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_set_style_flex_grow(receiveCoordsBtn, 1, 0);
    lv_obj_add_event_cb(receiveCoordsBtn, init_main_poc_page, LV_EVENT_CLICKED, NULL);

    lv_obj_t *receiveCoordsLabel = lv_label_create(receiveCoordsBtn);
    lv_label_set_text(receiveCoordsLabel, "Receive Pos");
    lv_obj_center(receiveCoordsLabel);

    lv_obj_t *uploadLogsBtn = lv_btn_create(cont);
    lv_obj_center(uploadLogsBtn);
    lv_obj_set_style_flex_grow(uploadLogsBtn, 1, 0);
    lv_obj_set_style_bg_color(uploadLogsBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(uploadLogsBtn, init_upload_log_page, LV_EVENT_CLICKED, NULL);

    lv_obj_t *uploadLogsLabel = lv_label_create(uploadLogsBtn);
    lv_label_set_text(uploadLogsLabel, "Upload Logs");
    lv_obj_center(uploadLogsLabel);
}


static std::vector<uint8_t> receivedFileBuffer;
static size_t expectedFileLength = 0;
static size_t expectedChunkSize = 0;
static uint16_t lastReceivedChunk = 0xFFFF;
static bool receivingFile = false;

static void dumpHex(const uint8_t* buf, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (buf[i] < 0x10) Serial.print('0');
        Serial.print(buf[i], HEX);
        Serial.print(' ');
    }
}

void finishTransfer()
{
    receivingFile = false;

    Serial.printf("\n--- received %u bytes ---\n", receivedFileBuffer.size());
    Serial.write(receivedFileBuffer.data(), receivedFileBuffer.size());
    Serial.println("\n-------------------------");

    dumpHex(receivedFileBuffer.data(), receivedFileBuffer.size());
}

void parseInitFrame(const char* pkt, size_t len)
{

    const size_t tagLen = strlen(kFileInitTag);
    if (len <= tagLen + 1 || pkt[tagLen] != ':') {
        Serial.println("INIT frame corrupted");
        return;
    }

    size_t fileLen    = 0;
    size_t chunkBytes = 0;
    int parsed = sscanf(pkt + tagLen, ":%zu:%zu", &fileLen, &chunkBytes);
    if (parsed != 2 || fileLen == 0 || chunkBytes == 0) {
        Serial.println("INIT numbers invalid");
        return;
    }

    expectedFileLength = fileLen;
    expectedChunkSize  = chunkBytes;

    receivedFileBuffer.clear();
    receivedFileBuffer.reserve(expectedFileLength);
    lastReceivedChunk  = 0xFFFF;
    receivingFile      = true;

    Serial.printf("INIT  fileLen=%u  chunkSize=%u\n",
                  (unsigned)expectedFileLength,
                  (unsigned)expectedChunkSize);
}

void onLoraFileDataReceived(const uint8_t* pkt, size_t len)
{
    Serial.println(len);
    if (len == 0) return;

    if (memcmp(pkt, kFileInitTag, strlen(kFileInitTag)) == 0) {
        parseInitFrame((const char*)pkt, len);
        return;
    }
    if (memcmp(pkt, kFileEndTag, strlen(kFileEndTag)) == 0) {
        finishTransfer();
        return;
    }

    handleFileChunk(pkt, len);
}

void handleFileChunk(const uint8_t* bytes, size_t len)
{   
    Serial.println("handleFileChunk");
    Serial.println(len);
    Serial.println(bytes[0]);

    if (len < 4 || bytes[0] != 0xAB) return;

    uint16_t chunkNum = (bytes[1] << 8) | bytes[2];
    uint8_t  chunkLen = bytes[3];

    Serial.println(chunkNum);
    Serial.println(chunkLen);

    if (chunkLen + 4 != len) return;         
    if (chunkNum != lastReceivedChunk + 1 && lastReceivedChunk != 0xFFFF)
        return;

    lastReceivedChunk = chunkNum;
    receivedFileBuffer.insert(receivedFileBuffer.end(),
                              bytes + 4, bytes + 4 + chunkLen);

    Serial.printf("chunk=%u len=%u total=%u/%u\n",
                  chunkNum, chunkLen,
                  receivedFileBuffer.size(), expectedFileLength);

    String chunkTxt((const char*)(bytes + 4), chunkLen);
    Serial.println(chunkTxt);
}


void writeToLogFile(const char* filePath, const char* content)
{
    File file = FFat.open(filePath, FILE_APPEND);

    if (!file) {
        // Serial.println("Failed to open file for writing!");
    } else {
        
        file.println(content);
        file.close();
    }
}

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

void setupCommander()
{
  
  if (!FFat.begin(true)) {
      Serial.println("Failed to mount FFat!");
      return;
  }

  listFiles();
  load_env();

  Serial.println("load_env");

  crypto::CryptoModule::init();
  Serial.println("init cryptoModule");
  

  deleteExistingFile(tileFilePath);


  beginLvglHelper();
  lv_png_init();
  Serial.println("LVGL set!");

  const char* ssid = env_map["WIFI_SSID"].c_str();
  const char* password = env_map["WIFI_PASS"].c_str();

  wifiModule = std::make_unique<WifiModule>(ssid, password);
  wifiModule->connect(60000);

  loraModule = std::make_unique<LoraModule>(433.5);
  loraModule->setup(false);
  loraModule->setOnReadData(init_p2p_test);

  Serial.println("After LORA INIT");

  init_main_menu();

  Serial.println(WiFi.localIP());
  setenv("TZ", "GMT-3", 1);
  tzset();

  configTime(0, 0, "pool.ntp.org");

  Serial.println("Configured time zone!");


  struct tm timeInfo;

  while (!getLocalTime(&timeInfo)) {
      Serial.println("Waiting for time sync...");
      delay(500);
  }

  if (getLocalTime(&timeInfo)) {
      char timeStr[20];
      strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeInfo);
      Serial.println(timeStr);
  }

  watch.attachPMU(onPmuInterrupt);
  watch.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);
  watch.clearPMU();
  watch.enableIRQ(
      XPOWERS_AXP2101_PKEY_SHORT_IRQ | 
      XPOWERS_AXP2101_PKEY_LONG_IRQ
  );
      
  loraModule->setupListening();
  Serial.println("End of setup");
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

  if (!cryptohelper.loadSingleCertificate(String((const char*)certRaw.data()))) {
    lv_label_set_text(statusLabel, "❌ Cert parse");
    return;
  }
  Serial.println("C");

  if (!cryptohelper.verifyCertificate()) {
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

  finished = true;

  clearMainPage();
  setupCommander();
}



// === Download config ===
void onDownloadPressed(lv_event_t* e) {
  wifi.connect(10000);
  if (!wifi.isConnected()) {
    lv_label_set_text(statusLabel, "WiFi fail");
    return;
  }

  HTTPClient http;
  http.begin("http://192.168.0.44:5555/download/watch1");
  int code = http.GET();
  if (code == 200) {
    String json = http.getString();
    config = new CommanderConfigModule(json);
    if (!cryptohelper.loadFromConfig(*config)) {
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
  if(!finished)
  {
    lora.readData();

    if (waitingResponse && millis() - startWaitTime > TIMEOUT_MS) {
      lv_label_set_text(statusLabel, "❌ Timeout");
      waitingResponse = false;
    }
  }
  else
  {
    if (pmu_flag) {
        pmu_flag = false;
        uint32_t status = watch.readPMU();
        if (watch.isPekeyShortPressIrq()) {
            init_main_menu();
        }
        watch.clearPMU();
    }
  
    loraModule->readData();
  }
  
}