#include <WiFi.h>
#include <WiFiUdp.h>


void connectToWiFi(const char *ssid, const char *pwd, void(*onConnectEvent) (WiFiEvent_t));

void disconnectFromWiFi();

