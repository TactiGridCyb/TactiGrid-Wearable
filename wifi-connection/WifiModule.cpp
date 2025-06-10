#include "WifiModule.h"

WifiModule::WifiModule(String wifiName, String password) : wifiName(wifiName), password(password)
{
    
}



JsonDocument WifiModule::receiveJSONTCP(const char* serverIP, uint16_t serverPort, uint32_t timeoutMs) {
    Serial.println("receiveJSONTCP");
    WiFiClientSecure client;
    client.setInsecure();
    
    if (!client.connect(serverIP, serverPort)) {
        Serial.println("❌ TCP connection failed");
        return JsonDocument();
    }

    JsonDocument doc;

    client.setTimeout(timeoutMs / 100);

    Serial.println("setTimeout");
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

void WifiModule::connect(uint32_t timeoutMillisec)
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

bool WifiModule::sendString(const String& data,
                            const char* destinationIP,
                            uint16_t destinationPort)
{
    WiFiClientSecure client;
    client.setInsecure();  
    Serial.printf("sendString → %s:%u\n", destinationIP, destinationPort);
    if (!client.connect(destinationIP, destinationPort)) {
        Serial.println("  ✗ connect failed");
        return false;
    }

    client.print(data);
    client.flush();
    client.stop();
    Serial.println("  ✓ sent");
    return true;
}

bool WifiModule::sendStringPost(const char* site, const String& data, uint16_t destinationPort)
{
    HTTPClient https;
    WiFiClientSecure *client = new WiFiClientSecure;

    client->setInsecure();

    String url = String(site);

    https.begin(*client, url);
    https.addHeader("Content-Type", "application/json");

    int httpCode = https.POST(data);

    if (httpCode > 0) 
    {
        Serial.printf("✓ POST sent to %s — HTTP code: %d\n", url.c_str(), httpCode);
        String response = https.getString();
        Serial.println("Response:");
        Serial.println(response);
    } else 
    {
        Serial.printf("✗ POST request failed — error: %s\n", https.errorToString(httpCode).c_str());
        delete client;
        return false;
    }

    https.end();
    delete client;
    return true;
}

bool WifiModule::sendFile(const String& filePath,
                          const char* destinationIP,
                          uint16_t destinationPort)
{
    WiFiClientSecure client;
    client.setInsecure();  
    Serial.printf("sendFile(%s) → %s:%u\n", filePath.c_str(), destinationIP, destinationPort);

    if (!client.connect(destinationIP, destinationPort)) {
        Serial.println("  ✗ connect failed");
        return false;
    }

    File f = FFat.open(filePath.c_str(), FILE_READ);
    if (!f) {
        Serial.printf("  ✗ failed to open %s\n", filePath.c_str());
        client.stop();
        return false;
    }

    uint8_t buf[256];
    while (f.available()) {
        size_t n = f.read(buf, sizeof(buf));
        client.write(buf, n);
    }
    
    f.close();

    client.flush();
    client.stop();
    Serial.println("  ✓ file sent");
    return true;
}