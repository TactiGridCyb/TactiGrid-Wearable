#include "WifiModule.h"

WifiModule::WifiModule(String wifiName, String password) : wifiName(wifiName), password(password)
{
    
}


JsonDocument WifiModule::receiveJSONTCP(const char* serverIP, uint16_t serverPort, uint32_t timeoutMs) {
    WiFiClientSecure client;
    client.setInsecure();
    
    if (!client.connect(serverIP, serverPort)) {
        Serial.println("❌ TCP connection failed");
        return JsonDocument();
    }

    JsonDocument doc;

    client.setTimeout(timeoutMs / 100);


    DeserializationError error = deserializeJson(doc, client);
    if (error) {
        Serial.print("❌ JSON parse failed: ");
        Serial.println(error.c_str());

        return JsonDocument();
    }

    serializeJsonPretty(doc, Serial);
    Serial.println();


    client.stop();

    return doc;
}

bool WifiModule::isConnected()
{
    return this->connected;
}

bool WifiModule::downloadFile(const char* donwloadLink, const char* fileName)
{
    HTTPClient http;
    http.begin(donwloadLink);
    http.addHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                                "AppleWebKit/537.36 (KHTML, like Gecko) "
                                "Chrome/134.0.0.0 Safari/537.36");
    http.addHeader("Referer", "https://www.iconduck.com/");
    http.addHeader("Accept", "image/png");

    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        size_t fileSize = http.getSize();
        WiFiClient* stream = http.getStreamPtr();
        uint8_t* buffer = new uint8_t[fileSize];
        stream->readBytes(buffer, fileSize);
        bool success = FFatHelper::saveFile(buffer, fileSize, fileName);
        delete[] buffer;
        http.end();
        return success;
    } else {
        http.end();
        return false;
    }
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