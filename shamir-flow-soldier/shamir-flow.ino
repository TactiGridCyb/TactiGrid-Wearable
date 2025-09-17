// #include <LilyGoLib.h>
// #include <LV_Helper.h>
// #include <RadioLib.h>
// #include <FFat.h>
// #include <WiFi.h>
// #include <WiFiUdp.h>
// #include <lvgl.h>

// // === LoRa Setup ===
// SX1262 lora = newModule();

// // === WiFi & UDP ===
// const char* ssid = "peretz1";
// const char* password = "box17box";
// const char* udp_ip = "192.168.1.71";
// const int udp_port = 5555;
// WiFiUDP udp;

// // === UI ===
// lv_obj_t* udp_btn;
// bool messageReceived = false;

// // === Function Prototypes ===
// void initializeLoRa();
// bool receiveMessage(String& message);
// void writeToFile(const String& content);
// void uploadFileViaUDP();
// void on_udp_btn_event(lv_event_t* e);

// // === Setup ===
// void setup() {
//   Serial.begin(115200);
//   delay(1000);

//   watch.begin();
//   beginLvglHelper();

//   if (!FFat.begin()) {
//     Serial.println("Mounting FFat failed. Formatting...");
//     FFat.format();
//     FFat.begin();
//   }
//   FFat.remove("/share2.txt");

//   initializeLoRa();

//   // Connect WiFi
//   WiFi.begin(ssid, password);
//   Serial.print("Connecting to WiFi");
//   while (WiFi.status() != WL_CONNECTED) {
//     delay(500);
//     Serial.print(".");
//   }
//   Serial.println();
//   Serial.println("WiFi connected, IP: " + WiFi.localIP().toString());
//   udp.begin(WiFi.localIP(), udp_port);

//   // Button to upload the received file
//   lv_obj_t* label;
//   udp_btn = lv_btn_create(lv_scr_act());
//   lv_obj_set_size(udp_btn, 200, 50);
//   lv_obj_align(udp_btn, LV_ALIGN_TOP_MID, 0, 20);
//   lv_obj_add_event_cb(udp_btn, on_udp_btn_event, LV_EVENT_CLICKED, NULL);

//   label = lv_label_create(udp_btn);
//   lv_label_set_text(label, "Upload Share2 via UDP");
//   lv_obj_center(label);
// }

// // === Loop ===
// void loop() {
//   lv_task_handler();

//   if (!messageReceived) {
//     String incoming;
//     if (receiveMessage(incoming)) {
//       if (incoming == "upload_ended") {
//         Serial.println("[LoRa] Upload ended message received.");
//         messageReceived = true;
//       } else {
//         writeToFile(incoming + "\n");  // Append with newline
//         Serial.println("[LoRa] Line written to file.");
//       }
//     }
//   }

//   delay(200);  // Slight delay to avoid tight loop
// }

// // === LoRa Init ===
// void initializeLoRa() {
//   Serial.print("[LoRa] Initializing... ");
//   int state = lora.begin();
//   if (state == RADIOLIB_ERR_NONE) {
//     Serial.println("success!");
//   } else {
//     Serial.print("failed, code ");
//     Serial.println(state);
//     while (true);
//   }

//   lora.setFrequency(868.0);
//   lora.setOutputPower(10);
//   lora.setSpreadingFactor(7);
//   lora.setBandwidth(125.0);
//   lora.setCodingRate(5);
// }

// // === Receive a single full message ===
// bool receiveMessage(String& message) {
//   int state = lora.receive(message);
//   if (state == RADIOLIB_ERR_NONE) {
//     return true;
//   } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
//     Serial.println("[LoRa] Timeout");
//   } else {
//     Serial.print("[LoRa] Receive error: ");
//     Serial.println(state);
//   }
//   return false;
// }

// // === Save content as a full file ===
// void writeToFile(const String& content) {
//   File file = FFat.open("/share2.txt", FILE_APPEND);  // FILE_APPEND so we accumulate data
//   if (!file) {
//     Serial.println("❌ Failed to open share2.txt for writing");
//     return;
//   }
//   file.print(content);  // content already includes newline
//   file.close();
//   Serial.println("✅ Line appended to share2.txt");
// }

// // === Upload via UDP on button press ===
// void on_udp_btn_event(lv_event_t* e) {
//   uploadFileViaUDP();
// }
// void uploadFileViaUDP() {
//   File s2 = FFat.open("/share2.txt");
//   if (!s2) {
//     Serial.println("❌ Failed to open share2.txt");
//     return;
//   }

//   const size_t CHUNK_SIZE = 1024;
//   const size_t MAX_CHUNKS = 100;  // Adjust if your files can be larger
//   char buffer[CHUNK_SIZE + 1];
//   String packets[MAX_CHUNKS];
//   int packetCount = 0;

//   // === Read file into array ===
//   while (s2.available() && packetCount < MAX_CHUNKS) {
//     size_t len = s2.readBytes(buffer, CHUNK_SIZE);
//     buffer[len] = '\0';
//     packets[packetCount++] = String(buffer);
//   }
//   s2.close();

//   Serial.printf("📦 Total packets to send: %d\n", packetCount);

//   // === Send each packet ===
//   for (int i = 0; i < packetCount; i++) {
//     String seqPacket = "SEQ:" + String(i + 1) + "/" + String(packetCount) + "\n" + packets[i];

//     udp.beginPacket(udp_ip, udp_port);
//     udp.write((const uint8_t*)seqPacket.c_str(), seqPacket.length());
//     udp.endPacket();

//     Serial.printf("✅ Sent packet %d/%d\n", i + 1, packetCount);
//     delay(100);
//   }

//   // === Send EOF packet ===
//   udp.beginPacket(udp_ip, udp_port);
//   udp.print("EOF\n");
//   udp.endPacket();
//   Serial.println("✅ Final EOF packet sent.");
// }


#include <LilyGoLib.h>
#include <RadioLib.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <FFat.h>
#include <lvgl.h>
#include <LV_Helper.h>

#define SHARES 2
#define THRESHOLD 2
#define PRIME 257

const char* ssid = "*******";
const char* password = "*******";
const char* udp_ip = "192.168.1.71";
const int udp_port = 5555;

lv_obj_t* udp_btn;

WiFiUDP udp;

// === Polynomial evaluation for Shamir's algorithm ===
uint8_t evalPolynomial(uint8_t x, uint8_t s, uint8_t r) {
  return (s + r * x) % PRIME;
}

// === Embed the config.json ===
void createRealJson() {
  const char* jsonContent = R"rawliteral(
{
  "operation": "OPERATION TACTIGRID",
  "missionId": "ABC1234",
  "Duration": 87,
  "GMK": "mission_map.gmk",
  "Location": {
    "name": "Base Beta",
    "bbox": [34.7878, 32.085, 34.789, 32.086]
  },
  "Soldiers": [
    {"id": "john", "callsign": "John"},
    {"id": "sarah", "callsign": "Sarah"},
    {"id": "alex", "callsign": "Alex"}
  ]
}
)rawliteral";

  File file = FFat.open("/config.json", FILE_WRITE);
  if (!file) {
    Serial.println("❌ Failed to create config.json");
    return;
  }
  file.print(jsonContent);
  file.close();
  Serial.println("✅ config.json created from embedded text.");
}

// === Split config.json into share1.txt and share2.txt ===
void splitJsonFile() {
  File file = FFat.open("/config.json");
  if (!file) {
    Serial.println("❌ Failed to open config.json");
    return;
  }

  String share1 = "";
  String share2 = "";

  while (file.available()) {
    uint8_t s = file.read();
    uint8_t r = random(1, PRIME);
    share1 += String(1) + "," + String(evalPolynomial(1, s, r)) + "\n";
    share2 += String(2) + "," + String(evalPolynomial(2, s, r)) + "\n";
  }
  file.close();

  File s1 = FFat.open("/share1.txt", FILE_WRITE);
  s1.print(share1);
  s1.close();

  File s2 = FFat.open("/share2.txt", FILE_WRITE);
  s2.print(share2);
  s2.close();

  Serial.println("✅ Split complete: share1.txt and share2.txt written.");
}

// === Upload share2.txt in UDP chunks ===
void uploadFileViaUDP() {
  File s2 = FFat.open("/share2.txt");
  if (!s2) {
    Serial.println("❌ Failed to open share2.txt");
    return;
  }

  const size_t CHUNK_SIZE = 1024;
  const size_t MAX_CHUNKS = 100;
  char buffer[CHUNK_SIZE + 1];
  String packets[MAX_CHUNKS];
  int packetCount = 0;

  while (s2.available() && packetCount < MAX_CHUNKS) {
    size_t len = s2.readBytes(buffer, CHUNK_SIZE);
    buffer[len] = '\0';
    packets[packetCount++] = String(buffer);
  }
  s2.close();

  Serial.printf("📦 Total packets to send: %d\n", packetCount);

  for (int i = 0; i < packetCount; i++) {
    String seqPacket = "SEQ:" + String(i + 1) + "/" + String(packetCount) + "\n" + packets[i];
    udp.beginPacket(udp_ip, udp_port);
    udp.write((const uint8_t*)seqPacket.c_str(), seqPacket.length());
    udp.endPacket();
    Serial.printf("✅ Sent packet %d/%d\n", i + 1, packetCount);
    delay(100);
  }

  udp.beginPacket(udp_ip, udp_port);
  udp.print("EOF\n");
  udp.endPacket();
  Serial.println("✅ Final EOF packet sent.");
}

// === Handle button click ===
void on_udp_btn_event(lv_event_t* e) {
  uploadFileViaUDP();
}

// === WiFi Event Handler ===
void WiFiEvent(WiFiEvent_t event) {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.print("WiFi connected! IP address: ");
    Serial.println(WiFi.localIP());
    udp.begin(WiFi.localIP(), udp_port);
  }
}

void setup() {
  Serial.begin(115200);
  watch.begin(&Serial);
  beginLvglHelper();
  delay(2000);
  randomSeed(micros());

  if (!FFat.begin()) {
    Serial.println("Mounting FFat failed. Formatting...");
    FFat.format();
    FFat.begin();
  }

  createRealJson();
  splitJsonFile();

  WiFi.disconnect(true, true);
  WiFi.onEvent(WiFiEvent);
  WiFi.begin(ssid, password);

  Serial.println("Waiting for WIFI connection...");
  int status = WiFi.waitForConnectResult();
  Serial.println(status);

  lv_obj_t* label;
  udp_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(udp_btn, 200, 50);
  lv_obj_align(udp_btn, LV_ALIGN_TOP_MID, 0, 50);
  lv_obj_add_event_cb(udp_btn, on_udp_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(udp_btn);
  lv_label_set_text(label, "Upload Share2 via UDP");
  lv_obj_center(label);
}

void loop() {
  lv_task_handler();
}

