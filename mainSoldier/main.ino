#include "../mainPage/SoldiersMainPage.h"
#include "../receiveParametersPage/SoldiersReceiveParametersPage.h"
#include "../missionPage/SoldiersMissionPage.h"
#include <CommandersMissionPage.h>
#include <LoraModule.h>
#include <GPSModule.h>
#include <FHFModule.h>
#include "../env.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

void transferFromSendCoordsToReceiveCoordsPage(std::shared_ptr<LoraModule> newLoraModule, 
    std::shared_ptr<GPSModule> newGPSModule, std::unique_ptr<FHFModule> newFHFModule,
    std::unique_ptr<Commander> commandersModule);

std::unique_ptr<SoldiersReceiveParametersPage> receiveParametersPage;
std::unique_ptr<SoldiersMainPage> soldiersMainPage;
std::unique_ptr<SoldiersMissionPage> soldiersMissionPage;
std::unique_ptr<CommandersMissionPage> commandersMissionPage;

std::unique_ptr<WifiModule> wifiModule;
std::shared_ptr<LoraModule> loraModule;
std::shared_ptr<GPSModule> gpsModule;
std::unique_ptr<FHFModule> fhfModule;

std::unique_ptr<Soldier> soldiersModule;

const std::string logFilePath = "/log.txt";

void transferFromMissionCommanderToMissionSoldier(std::shared_ptr<LoraModule> newLoraModule,
     std::unique_ptr<WifiModule> newWifiModule,
     std::shared_ptr<GPSModule> newGPSModule, std::unique_ptr<FHFModule> newFhfModule,
     std::unique_ptr<Soldier> soldiersModule)
{
    
    
    Serial.println("transferFromMissionCommanderToMissionSoldier");

    wifiModule = std::move(newWifiModule);

    soldiersMissionPage = std::make_unique<SoldiersMissionPage>(newLoraModule,
     newGPSModule, std::move(newFhfModule), std::move(soldiersModule));

    Serial.println("std::make_unique<SoldiersMissionPage>");

    soldiersMissionPage->createPage();
    Serial.println("soldiersMissionPage->createPage()");

    commandersMissionPage.reset();

}

void transferFromSendCoordsToReceiveCoordsPage(std::shared_ptr<LoraModule> newLoraModule, 
    std::shared_ptr<GPSModule> newGPSModule, std::unique_ptr<FHFModule> newFHFModule,
    std::unique_ptr<Commander> commandersModule)
{
    

    commandersMissionPage = std::make_unique<CommandersMissionPage>(newLoraModule, 
    std::move(wifiModule), newGPSModule, std::move(newFHFModule), std::move(commandersModule), logFilePath);

    Serial.println("commandersMissionPage->createPage()");
    commandersMissionPage->createPage();

    Serial.println("commandersMissionPage->createPage() finished");
    commandersMissionPage->setTransferFunction(transferFromMissionCommanderToMissionSoldier);

    soldiersMissionPage.reset();
    Serial.println("DELETED ALL");
}

void transferFromMainToSendCoordsPage(std::unique_ptr<WifiModule> currentWifiModule)
{
    Serial.println("transferFromMainToSendCoordsPage");
    loraModule = std::make_shared<LoraModule>(433.5);
    gpsModule = std::make_shared<GPSModule>();

    loraModule->setup(true);

    fhfModule = std::make_unique<FHFModule>(soldiersModule->getFrequencies());

    wifiModule = std::move(currentWifiModule);

    soldiersMissionPage = std::make_unique<SoldiersMissionPage>(loraModule, gpsModule, std::move(fhfModule), std::move(soldiersModule));
    
    soldiersMissionPage->createPage();

    soldiersMissionPage->setTransferFunction(transferFromSendCoordsToReceiveCoordsPage);
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
    lv_png_init();
    
    crypto::CryptoModule::init();

    if (!FFat.begin(true)) 
    {
        Serial.println("Failed to mount FFat!");
        return;
    }
    
    FFatHelper::removeFilesIncludeWords("share", "txt");

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