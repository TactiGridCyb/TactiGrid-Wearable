#include "../mainPage/SoldiersMainPage.h"
#include "../receiveParametersPage/SoldiersReceiveParametersPage.h"
#include <SoldierSendCoordsPage.h>
#include <LoraModule.h>
#include <GPSModule.h>

const char* ssid = "default";
const char* password = "1357924680";

std::unique_ptr<SoldiersReceiveParametersPage> receiveParametersPage;
std::unique_ptr<SoldiersMainPage> soldiersMainPage;
std::unique_ptr<SoldierSendCoordsPage> soldierSendCoordsPage;

std::unique_ptr<WifiModule> wifiModule;
std::shared_ptr<LoraModule> loraModule;
std::shared_ptr<GPSModule> gpsModule;


void transferFromMainToSendCoordsPage(std::unique_ptr<WifiModule> currentWifiModule)
{
    Serial.println("transferFromMainToSendCoordsPage");
    loraModule = std::make_shared<LoraModule>(433.5);
    gpsModule = std::make_shared<GPSModule>();

    loraModule->setup(true);

    soldierSendCoordsPage = std::make_unique<SoldierSendCoordsPage>(loraModule, std::move(currentWifiModule), gpsModule);
    soldierSendCoordsPage->createPage();
}

void transferFromReceiveParametersToMainPage(std::unique_ptr<WifiModule> currentWifiModule)
{
    Serial.println("transferFromReceiveParametersToMainPage");
    soldiersMainPage = std::make_unique<SoldiersMainPage>(std::move(currentWifiModule));
    soldiersMainPage->createPage();

    soldiersMainPage->setOnTransferPage(transferFromMainToSendCoordsPage);
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

    struct tm timeInfo;
    setenv("TZ", "GMT-3", 1);
    tzset();
    

    configTime(0, 0, "pool.ntp.org");
    while (!getLocalTime(&timeInfo)) {
        Serial.println("Waiting for time sync...");
        delay(500);
    }
    
    receiveParametersPage = std::make_unique<SoldiersReceiveParametersPage>(std::move(wifiModule));
    receiveParametersPage->createPage();
    receiveParametersPage->setOnTransferPage(transferFromReceiveParametersToMainPage);

}


void loop()
{
    lv_task_handler();
    delay(5);

}