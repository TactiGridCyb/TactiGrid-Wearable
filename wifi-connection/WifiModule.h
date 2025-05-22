#pragma once

#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

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
    void connect(uint32_t);
    void disconnect();
    void sendFile(String filePath, const char* destinationIP, uint16_t destinationPort);
    void sendString(String data, const char* destinationIP, uint16_t destinationPort);

    bool isConnected();

    DynamicJsonDocument receiveJSONTCP(const char* serverIP, uint16_t serverPort, uint32_t timeoutMs = 5000);
};
