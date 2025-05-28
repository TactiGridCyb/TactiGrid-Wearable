// #include <WiFi.h>
// #include <WiFiUdp.h>

// #include "LilyGoLib.h"

// const char *networkName = "XXX";
// const char *networkPswd = "XXX";

// const char *udpAddress = "192.168.0.133";
// const int udpPort = 3333;

// boolean connected = false;

// WiFiUDP udp;

// void setup() {
//   Serial.begin(115200);
//   watch.begin(&Serial);

//   connectToWiFi(networkName, networkPswd);
//   Serial.println("Starting to send messages");


//   for (size_t i = 0; i < 10; i++)
//   {
//     Serial.println("Message number " + String(i));
//     udp.beginPacket(udpAddress, udpPort);
//     udp.printf("Seconds since boot: %lu", millis() / 1000);
//     udp.endPacket();
//     delay(500);
//   }

//   WiFi.disconnect(true, true);

//   Serial.printf("Disconnected!");
// }

// void loop() {}

// void connectToWiFi(const char *ssid, const char *pwd) {
//   Serial.println("Connecting to WiFi network: " + String(ssid));

//   WiFi.disconnect(true, true);

//   WiFi.onEvent(WiFiEvent);
  
//   WiFi.begin(ssid, pwd);

//   Serial.println("Waiting for WIFI connection...");
//   int status = WiFi.waitForConnectResult();
//   Serial.println(status);
// }

// void WiFiEvent(WiFiEvent_t event) {
//   switch (event) {
//     case ARDUINO_EVENT_WIFI_STA_GOT_IP:

//       Serial.print("WiFi connected! IP address: ");
//       Serial.println(WiFi.localIP());

//       udp.begin(WiFi.localIP(), udpPort);
//       connected = true;

//       break;
//     // case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
//     //   Serial.println("WiFi lost connection");
//     //   connected = false;
//     //   break;
//     default: break;
//   }
// }