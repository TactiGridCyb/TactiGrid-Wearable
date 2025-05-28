#include <SoldiersMainPage.h>
#include <SoldiersReceiveParametersPage.h>

const char* ssid = "default";
const char* password = "1357924680";

std::unique_ptr<SoldiersReceiveParametersPage> receiveParametersPage;
std::unique_ptr<SoldiersMainPage> soldiersMainPage;
std::unique_ptr<WifiModule> wifiModule;

void transferFromReceiveParametersToMainPage(std::unique_ptr<WifiModule> currentWifiModule)
{
    Serial.println("transferFromReceiveParametersToMainPage");
    soldiersMainPage = std::make_unique<SoldiersMainPage>(std::move(currentWifiModule));
    soldiersMainPage->createPage();
}

void setup()
{
    Serial.begin(115200);
    watch.begin(&Serial);

    beginLvglHelper();
    
    String ssidString(ssid);
    String passwordString(password);

    wifiModule = std::make_unique<WifiModule>(ssidString, passwordString);
    wifiModule->connect(10000);
    if(wifiModule->isConnected())
    {
        Serial.println("Connected to wifi!");
    }
    else
    {
        Serial.println("Failed to connect to wifi!");
    }
    
    receiveParametersPage = std::make_unique<SoldiersReceiveParametersPage>(std::move(wifiModule));
    receiveParametersPage->createPage();
    receiveParametersPage->setOnTransferPage(transferFromReceiveParametersToMainPage);

}


void loop()
{
    lv_timer_handler();
    delay(5);

}