#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "../FFatHelper/FFatHelper.h"

class WifiModule {
    private:
    bool connected = false;
    uint16_t port = 3333;

    WiFiUDP udpModule;
    IPAddress ipAddress;

    String wifiName;
    String password;

    WiFiUDP udp;

    public:
    WifiModule(String wifiName, String password);
    void connect(uint32_t = 0);
    void disconnect();
    void sendFile(String filePath, const char* destinationIP, uint16_t destinationPort);
    void sendString(String data, const char* destinationIP, uint16_t destinationPort);

    bool isConnected();

    bool downloadFile(const char* downloadLink, const char* fileName);

    JsonDocument receiveJSONTCP(const char* serverIP, uint16_t serverPort, uint32_t timeoutMs = 10000);
};
