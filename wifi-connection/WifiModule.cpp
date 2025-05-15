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
    
}