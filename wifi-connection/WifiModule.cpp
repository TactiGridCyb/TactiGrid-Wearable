#include "WifiModule.h"

WifiModule::WifiModule(String wifiName, String password) : wifiName(wifiName), password(password)
{
    
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
    if (!this->connected) {
        Serial.println("WiFi not connected. Cannot send file.");
        return;
    }

    File file = FFat.open(filePath, FILE_READ);
    if (!file) {
        Serial.printf("Failed to open file: %s\n", filePath.c_str());
        return;
    }

    const size_t chunkSize = 512;
    uint8_t buffer[chunkSize];
    
    while (file.available()) {
        size_t bytesRead = file.read(buffer, chunkSize);
        if (bytesRead > 0) {
            this->udp.beginPacket(destinationIP, destinationPort);
            this->udp.write(buffer, bytesRead);
            this->udp.endPacket();
            delay(10);
        }
    }

    file.close();

    // this->udp.beginPacket(destinationIP, destinationPort);
    // this->udp.println("[EOF]");
    // this->udp.endPacket();

    //If you want, you can remove it. It might help with telling when the file was full transfered.

    Serial.printf("Finished sending file: %s\n", filePath.c_str());
}