#include <SoldiersMainPage.h>
#include <SoldiersReceiveParametersPage.h>

const char* ssid = "default";
const char* password = "1357924680";

void setup()
{
    Serial.begin(115200);
    watch.begin(&Serial);

    beginLvglHelper();
    
    String ssidString(ssid);
    String passwordString(password);

    std::unique_ptr<WifiModule> wifiModule = std::make_unique<WifiModule>(ssidString, passwordString);
    SoldiersReceiveParametersPage receiveParametersPage(std::move(wifiModule));
    receiveParametersPage.createPage();

}


void loop()
{

}