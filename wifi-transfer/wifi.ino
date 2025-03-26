#include <WiFi.h>
#include <WiFiUdp.h>

const char *networkName = "XXX";
const char *networkPswd = "XXX";

const char *udpAddress = "192.168.0.181";
const int udpPort = 3333;

boolean connected = false;

WiFiUDP udp;

void setup() {
  Serial.begin(115200);

  connectToWiFi(networkName, networkPswd);

  for (size_t i = 0; i < 10; i++)
  {
    udp.beginPacket(udpAddress, udpPort);
    udp.printf("Seconds since boot: %lu", millis() / 1000);
    udp.endPacket();
  }


}

void loop() {}

void connectToWiFi(const char *ssid, const char *pwd) {
  Serial.println("Connecting to WiFi network: " + String(ssid));

  WiFi.disconnect(true);

  WiFi.onEvent(WiFiEvent);

  WiFi.begin(ssid, pwd);

  Serial.println("Waiting for WIFI connection...");
}

void WiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:

      Serial.print("WiFi connected! IP address: ");
      Serial.println(WiFi.localIP());

      udp.begin(WiFi.localIP(), udpPort);
      connected = true;

      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      connected = false;
      break;
    default: break;
  }
}