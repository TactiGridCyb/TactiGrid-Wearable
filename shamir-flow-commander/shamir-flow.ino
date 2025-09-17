#include <LilyGoLib.h>
#include <RadioLib.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <FFat.h>
#include <lvgl.h>
#include <LV_Helper.h>

#define SHARES 2
#define THRESHOLD 2
#define PRIME 257

const char* ssid = "********";
const char* password = "********";
const char* udp_ip = "192.168.1.71";
const int udp_port = 5555;

lv_obj_t* split_btn;
lv_obj_t* udp_btn;

String share1;
String share2;

WiFiUDP udp;
SX1262 lora = newModule();

// Polynomial evaluation for Shamir's algorithm
uint8_t evalPolynomial(uint8_t x, uint8_t s, uint8_t r) {
  return (s + r * x) % PRIME;
}

void createRealJson() {
  const char* jsonContent = R"rawliteral(
{
  "_id": {
    "$oid": "6815f4b428c877956914cb72"
  },
  "userId": "680606c47d60c0b44df5b13d",
  "sessionId": "sess-fake-5346269364780",
  "operation": "OPERATION TACTIGRID",
  "missionId": "ABC1234",
  "StartTime": {
    "$date": "2025-05-05T08:00:00.000Z"
  },
  "EndTime": {
    "$date": "2025-05-05T08:01:27.000Z"
  },
  "Duration": 87,
  "GMK": "mission_map.gmk",
  "Location": {
    "name": "Base Beta",
    "bbox": [34.7878, 32.085, 34.789, 32.086]
  },
  "Soldiers": [
    {"id": "john", "callsign": "John", "_id": {"$oid": "6815f4b428c877956913aa73"}},
    {"id": "sarah", "callsign": "Sarah", "_id": {"$oid": "6815f4b428c877956913aa74"}},
    {"id": "alex", "callsign": "Alex", "_id": {"$oid": "6815f4b428c877956913aa75"}}
  ],
  "ConfigID": "config_v2",
  "intervalMs": 3000,
  "codec": {
    "path": "polyline",
    "hr": "delta‑varint",
    "compression": "brotli"
  },
  "data": [
    {"soldierId": "john", "latitude": 32.0851, "longitude": 34.788, "heartRate": 85, "time_sent": {"$date": "2025-03-30T08:00:00.000Z"}},
    {"soldierId": "sarah", "latitude": 32.08525, "longitude": 34.78825, "heartRate": 86, "time_sent": {"$date": "2025-03-30T08:00:03.000Z"}},
    {"soldierId": "alex", "latitude": 32.0854, "longitude": 34.7885, "heartRate": 87, "time_sent": {"$date": "2025-03-30T08:00:06.000Z"}},
    ...
    {"soldierId": "alex", "latitude": 32.08675, "longitude": 34.78985, "heartRate": 86, "time_sent": {"$date": "2025-03-30T08:01:27.000Z"}}
  ],
  "blob": {
    "$binary": {
      "base64": "iwaAZGVtb+KAkXBheWxvYWQD",
      "subType": "00"
    }
  },
  "createdAt": {"$date": "2025-05-03T10:49:24.799Z"},
  "updatedAt": {"$date": "2025-05-03T10:49:24.799Z"},
  "__v": 0
}
)rawliteral";

  File file = FFat.open("/config.json", FILE_WRITE);
  if (!file) {
    Serial.println("Failed to create config.json");
    return;
  }
  file.print(jsonContent);
  file.close();
  Serial.println("config.json created from embedded text.");
}


void splitJsonFile() {
  File file = FFat.open("/config.json");
  if (!file) {
    Serial.println("Failed to open config.json");
    return;
  }

  share1 = "";
  share2 = "";

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

  Serial.println("Split complete, sending share2 over LoRa...");

  // Send share2 over LoRa line by line
  s2 = FFat.open("/share2.txt", FILE_READ);
  if (!s2) {
    Serial.println("Failed to open share2.txt for LoRa transmission.");
    return;
  }

  while (s2.available()) {
  String line = s2.readStringUntil('\n');
  int state = lora.transmit(line.c_str());
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("✅ Sent line over LoRa: " + line);
  } else {
    Serial.print("❌ LoRa transmit failed with code: ");
    Serial.println(state);
  }
    delay(100); // prevent overloading LoRa
    }
    s2.close();

    // Send final "upload_ended" message to signal end of transfer
    int endState = lora.transmit("upload_ended");
    if (endState == RADIOLIB_ERR_NONE) {
      Serial.println("📤 Sent final upload_ended marker.");
    } else {
      Serial.print("❌ Failed to send upload_ended, code: ");
      Serial.println(endState);
    }
}

void sendShare1UDP() {
  File s1 = FFat.open("/share1.txt");
  if (!s1) {
    Serial.println("Failed to open share1.txt");
    return;
  }

  String content = "";
  while (s1.available()) content += (char)s1.read();
  s1.close();

  WiFi.disconnect(true, true);
  WiFi.onEvent(WiFiEvent);
  WiFi.begin(ssid, password);

  Serial.println("Waiting for WIFI connection...");
  int status = WiFi.waitForConnectResult();
  Serial.println(status);

  if (WiFi.status() == WL_CONNECTED) {
    for (int i = 0; i < 1; i++) {
      Serial.println("Sending share1 packet: " + String(i));
      udp.beginPacket(udp_ip, udp_port);
      udp.print(content);
      udp.endPacket();
      delay(500);
    }
    WiFi.disconnect(true, true);
    Serial.println("UDP transmission complete and WiFi disconnected.");
  }
}

void WiFiEvent(WiFiEvent_t event) {
  if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
    Serial.print("WiFi connected! IP address: ");
    Serial.println(WiFi.localIP());
    udp.begin(WiFi.localIP(), udp_port);
  }
}

void on_split_btn_event(lv_event_t* e) {
  splitJsonFile();
}

void on_udp_btn_event(lv_event_t* e) {
  sendShare1UDP();
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

  int state = lora.begin();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.print("LoRa init failed, code ");
    Serial.println(state);
    while (true);
  }

  // LoRa tuning (must match between sender and receiver)
  lora.setFrequency(868.0);
  lora.setOutputPower(10);
  lora.setSpreadingFactor(7);
  lora.setBandwidth(125.0);
  lora.setCodingRate(5);

  Serial.println("LoRa init success");

  lv_obj_t* label;

  // Split and send LoRa button
  split_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(split_btn, 200, 50);
  lv_obj_align(split_btn, LV_ALIGN_TOP_MID, 0, 20);
  lv_obj_add_event_cb(split_btn, on_split_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(split_btn);
  lv_label_set_text(label, "Split and Send Share2");
  lv_obj_center(label);

  // Send UDP button
  udp_btn = lv_btn_create(lv_scr_act());
  lv_obj_set_size(udp_btn, 200, 50);
  lv_obj_align(udp_btn, LV_ALIGN_TOP_MID, 0, 90);
  lv_obj_add_event_cb(udp_btn, on_udp_btn_event, LV_EVENT_CLICKED, NULL);
  label = lv_label_create(udp_btn);
  lv_label_set_text(label, "Upload Share1 via UDP");
  lv_obj_center(label);
}

void loop() {
  lv_task_handler();
}

