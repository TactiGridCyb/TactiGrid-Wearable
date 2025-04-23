#include <WiFi.h>
#include <WiFiUdp.h>

class WifiModule {
    private:
    bool connected = false;
    uint16_t port = 3333;
    WiFiUDP udpModule;
    IPAddress ipAddress;

    public:
    WifiModule(String wifiName, String password);
    void connect();
    void disconnect();
    void sendFile(String filePath, String destinationIP, uint16_t destinationPort);
    void sendString(String data, String destinationIP, uint16_t destinationPort);
};
void connectToWiFi(const char *ssid, const char *pwd, void(*onConnectEvent) (WiFiEvent_t));

void disconnectFromWiFi();

