#include <Arduino.h>
#include <tuple>              // ✅ For std::tuple
#include "LilyGoLib.h"
#include <LV_Helper.h>

// LoRa setup
SX1262 lora = newModule();

// Alias for coordinate tuple
using GPSCoordTuple = std::tuple<float, float, float, float>;

// Function declarations
void initializeWatch();
void initializeLoRa();
bool receiveMessage(String &message);
GPSCoordTuple parseCoordinates(const String &message);
void black_box(const GPSCoordTuple &coords);

void setup() {
  Serial.begin(115200);
  delay(1000);

  initializeWatch();
  initializeLoRa();
}

void loop() {
  String incoming;
  if (receiveMessage(incoming)) {
    GPSCoordTuple coords = parseCoordinates(incoming);
    black_box(coords);
  }

  delay(2000);
  lv_task_handler();
}

// === Initialize Watch ===
void initializeWatch() {
  watch.begin();
  beginLvglHelper();
}

// === Initialize LoRa ===
void initializeLoRa() {
  Serial.print(F("[LoRa] Initializing... "));
  int state = lora.begin();
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("success!"));
  } else {
    Serial.print(F("failed, code "));
    Serial.println(state);
    while (true);
  }

  lora.setFrequency(868.0);
  lora.setOutputPower(10);
  lora.setSpreadingFactor(9);
  lora.setBandwidth(125.0);
  lora.setCodingRate(5);
}

// === Receive LoRa message ===
bool receiveMessage(String &message) {
  Serial.println(F("[LoRa] Listening for incoming message..."));
  int state = lora.receive(message);

  if (state == RADIOLIB_ERR_NONE) {
    Serial.println(F("[LoRa] Message received!"));
    Serial.print(F("[LoRa] Data: "));
    Serial.println(message);
    return true;
  } else if (state == RADIOLIB_ERR_RX_TIMEOUT) {
    Serial.println(F("[LoRa] Timeout – no message received."));
  } else {
    Serial.print(F("[LoRa] Receive failed, code "));
    Serial.println(state);
  }
  return false;
}

// === Parse into std::tuple ===
GPSCoordTuple parseCoordinates(const String &message) {
  float lat1 = 0, lon1 = 0, lat2 = 0, lon2 = 0;
  sscanf(message.c_str(), "%f,%f;%f,%f", &lat1, &lon1, &lat2, &lon2);
  return std::make_tuple(lat1, lon1, lat2, lon2);
}

// === Print tuple contents ===
void black_box(const GPSCoordTuple &coords) {
  float lat1, lon1, lat2, lon2;
  std::tie(lat1, lon1, lat2, lon2) = coords;

  Serial.println("[black_box] Tuple received:");
  Serial.printf("  GPS 1: %.4f, %.4f\n", lat1, lon1);
  Serial.printf("  GPS 2: %.4f, %.4f\n", lat2, lon2);
}

// ******************************printing on screen**********************************************

// #include <LilyGoLib.h>

// int counter = 1;
// int y = 0;  // Y position for text

// void setup() {
//   watch.begin();
//   watch.fillScreen(TFT_BLACK);
//   watch.setRotation(2);

//   // Print initial text
//   watch.setTextFont(4);
//   watch.setTextColor(TFT_PURPLE, TFT_BLACK);
//   watch.setCursor(0, y);
//   watch.println("Hello!");
//   y += 30;  // Move cursor down for next line
//   watch.println("Forget Bazov!");
//   y += 30;
// }

// void loop() {
//   if (counter <= 10) {
//     watch.setCursor(0, y);
//     watch.setTextColor(TFT_YELLOW, TFT_BLACK);
//     watch.print("Number: ");
//     watch.println(counter);
//     y += 30; // Move down for next line
//     counter++;
//     delay(1000);  // Wait 1 second before next
//   } else {
//     // Do nothing after 1-5
//     while (true) delay(100);
//   }
// }


