#include "../mainPage/SoldiersMainPage.h"
#include "../receiveParametersPage/SoldiersReceiveParametersPage.h"
#include "../missionPage/SoldiersMissionPage.h"
#include <LoraModule.h>
#include <GPSModule.h>
#include <FHFModule.h>
#include "../env.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

std::unique_ptr<SoldiersReceiveParametersPage> receiveParametersPage;
std::unique_ptr<SoldiersMainPage> soldiersMainPage;
std::unique_ptr<SoldiersMissionPage> soldiersMissionPage;

std::unique_ptr<WifiModule> wifiModule;
std::shared_ptr<LoraModule> loraModule;
std::shared_ptr<GPSModule> gpsModule;
std::unique_ptr<FHFModule> fhfModule;

std::unique_ptr<Soldier> soldiersModule;


void transferFromMainToSendCoordsPage(std::unique_ptr<WifiModule> currentWifiModule)
{
    Serial.println("transferFromMainToSendCoordsPage");
    loraModule = std::make_shared<LoraModule>(433.5);
    gpsModule = std::make_shared<GPSModule>();

    loraModule->setup(true);

    fhfModule = std::make_unique<FHFModule>(soldiersModule->getFrequencies());

    soldiersMissionPage = std::make_unique<SoldiersMissionPage>(loraModule, std::move(currentWifiModule), gpsModule, std::move(fhfModule), std::move(soldiersModule));
    soldiersMissionPage->createPage();
}

void transferFromReceiveParametersToMainPage(std::unique_ptr<WifiModule> currentWifiModule, std::unique_ptr<Soldier> soldierModule)
{
    Serial.println("transferFromReceiveParametersToMainPage");
    soldiersMainPage = std::make_unique<SoldiersMainPage>(std::move(currentWifiModule));
    soldiersMainPage->createPage();

    soldiersMainPage->setOnTransferPage(transferFromMainToSendCoordsPage);

    soldiersModule = std::move(soldierModule);
}



void setup()
{
    Serial.begin(115200);
    watch.begin(&Serial);

    beginLvglHelper();

    if (!FFat.begin(true)) 
    {
        Serial.println("Failed to mount FFat!");
        return;
    }
    
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