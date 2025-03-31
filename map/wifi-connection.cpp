// #include <wifi-connection.h>

// boolean connected = false;

// WiFiUDP udp;

// void connectToWiFi(const char *ssid, const char *pwd, void(*onConnectEvent) (WiFiEvent_t)) {
//   Serial.println("Connecting to WiFi network: " + String(ssid));

//   WiFi.disconnect(true, true);

//   WiFi.onEvent(onConnectEvent);
  
//   WiFi.begin(ssid, pwd);

//   Serial.println("Waiting for WIFI connection...");
//   int status = WiFi.waitForConnectResult();
//   Serial.println(status);
// }

// void disconnectFromWiFi()
// {
//     WiFi.disconnect(true, true);
// }

// // void WiFiEvent(WiFiEvent_t event) {
// //   switch (event) {
// //     case ARDUINO_EVENT_WIFI_STA_GOT_IP:

// //       Serial.print("WiFi connected! IP address: ");
// //       Serial.println(WiFi.localIP());

// //       udp.begin(WiFi.localIP(), udpPort);
// //       connected = true;

// //       break;

// //     default: break;
// //   }
// // }