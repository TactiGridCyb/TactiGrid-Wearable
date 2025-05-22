#include "WifiModule.h"

WifiModule::WifiModule(String wifiName, String password) : wifiName(wifiName), password(password)
{
    
}


DynamicJsonDocument WifiModule::receiveJSONTCP(const char* serverIP, uint16_t serverPort, uint32_t timeoutMs) {
    WiFiClient client;
    DynamicJsonDocument doc(1024);

    if (!client.connect(serverIP, serverPort)) {
        Serial.println("❌ TCP connection failed");
        return doc;
    }

    uint32_t startTime = millis();
    String receivedData = "";

    while (millis() - startTime < timeoutMs) {
        while (client.available()) {
            char c = client.read();
            receivedData += c;
        }
        if (receivedData.length() > 0) {
            break;
        }
        delay(10);
    }

    client.stop();

    DeserializationError error = deserializeJson(doc, receivedData);
    if (error) {
        Serial.print("❌ JSON parse failed: ");
        Serial.println(error.c_str());

        return DynamicJsonDocument(0);
    }

    return doc;
}

bool WifiModule::isConnected()
{
    return this->connected;
}

void WifiModule::connect(uint32_t timeoutMillisec = 0)
{

    WiFi.begin(this->wifiName, this->password);
    uint8_t status = WiFi.waitForConnectResult(timeoutMillisec);

    this->connected = status == WL_CONNECTED && this->udp.begin(this->ipAddress, this->port) == 1;

    if(this->connected)
    {
        this->ipAddress = WiFi.localIP();
    }

}

void WifiModule::disconnect()
{
    if(this->connected)
    {
        return;
    }


    this->connected = !WiFi.disconnect();
}

void WifiModule::sendString(String data, const char* destinationIP, uint16_t destinationPort)
{
    this->udp.beginPacket(destinationIP, destinationPort);

    this->udp.println(data);

    this->udp.endPacket();
}


void WifiModule::sendFile(String filePath, const char* destinationIP, uint16_t destinationPort)
{
    
}