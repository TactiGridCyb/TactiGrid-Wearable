#include <WifiModule.h>

void setup()
{
    WifiModule wifiInstance("wifi-name", "wifi-password"); // It can connect only to 2.4GHz wifi network.
    wifiInstance.connect(10000000);

    wifiInstance.sendString("HELLO WORLD!", "192.168.0.1", 3333);

    wifiInstance.sendFile("path in wearable", "192.168.0.1", 3333);
}


void loop()
{
    
}