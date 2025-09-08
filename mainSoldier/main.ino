#include "../mainPage/SoldiersMainPage.h"
#include "../receiveParametersPage/SoldiersReceiveParametersPage.h"
#include "../missionPage/SoldiersMissionPage.h"
#include <CommandersMissionPage.h>
#include <LoraModule.h>
#include <GPSModule.h>
#include <FHFModule.h>
#include <CommandersUploadLogPage.h>
#include "../env.h"
#include "../diffieHelmanPage/diffieHelmanPageSoldier.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

void transferFromSendCoordsToReceiveCoordsPage(std::shared_ptr<LoraModule> newLoraModule, 
    std::shared_ptr<GPSModule> newGPSModule, std::unique_ptr<FHFModule> newFHFModule,
    std::unique_ptr<Commander> commandersModule);

std::unique_ptr<SoldiersReceiveParametersPage> receiveParametersPage;
std::unique_ptr<SoldiersMainPage> soldiersMainPage;
std::unique_ptr<SoldiersMissionPage> soldiersMissionPage;
std::unique_ptr<CommandersMissionPage> commandersMissionPage;
std::unique_ptr<CommandersUploadLogPage> commandersUploadLogPage;
std::unique_ptr<DiffieHellmanPageSoldier> dhPage;

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
     newGPSModule, std::move(newFhfModule), std::move(soldiersModule), true, true);

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
    std::move(wifiModule), newGPSModule, std::move(newFHFModule), std::move(commandersModule), logFilePath, true);

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

    fhfModule = std::make_unique<FHFModule>(soldiersModule->getFrequencies(), (uint16_t)(soldiersModule->getIntervalMS() / 1000));

    wifiModule = std::move(currentWifiModule);

    soldiersMissionPage = std::make_unique<SoldiersMissionPage>(loraModule, gpsModule,
         std::move(fhfModule), std::move(soldiersModule), false, true);
    
    soldiersMissionPage->createPage();

    soldiersMissionPage->setTransferFunction(transferFromSendCoordsToReceiveCoordsPage);
}

void transferFromMainToUploadLogsPage(std::unique_ptr<WifiModule> currentWifiModule)
{
    commandersUploadLogPage = std::make_unique<CommandersUploadLogPage>(std::move(currentWifiModule), logFilePath);
    commandersUploadLogPage->createPage();

    receiveParametersPage.reset();
}

// void transferFromReceiveParametersToMainPage(std::unique_ptr<WifiModule> currentWifiModule, std::unique_ptr<Soldier> soldierModule)
// {
//     Serial.println("transferFromReceiveParametersToMainPage");
//     soldiersMainPage = std::make_unique<SoldiersMainPage>(std::move(currentWifiModule));
//     soldiersMainPage->createPage();

//     soldiersMainPage->setOnTransferPage(transferFromMainToSendCoordsPage);

//     soldiersModule = std::move(soldierModule);
// }

void transferFromDhToMainPage(std::unique_ptr<WifiModule> currentWifiModule, std::unique_ptr<Soldier> soldierModule)
{
    Serial.println("transferFromDhToMainPage");

    soldiersMainPage = std::make_unique<SoldiersMainPage>(std::move(currentWifiModule));
    soldiersMainPage->createPage();

    soldiersMainPage->setOnTransferPage(transferFromMainToSendCoordsPage);

    soldiersModule = std::move(soldierModule);

    lv_async_call([](void * user_data) {
        auto* iDHPage = static_cast<std::unique_ptr<DiffieHellmanPageSoldier>*>(user_data);
        iDHPage->reset();

    }, &dhPage);

}

void transferFromReceiveParametersToDhPage(std::unique_ptr<WifiModule> currentWifiModule, std::unique_ptr<Soldier> soldierModule)
{
    Serial.println("transferFromReceiveParametersToDhPage");

    wifiModule = std::move(currentWifiModule);

    dhPage = std::make_unique<DiffieHellmanPageSoldier>(std::move(wifiModule), std::move(soldierModule), soldierModule.get());
    dhPage->createPage();

    dhPage->setOnTransferMainPage(transferFromDhToMainPage);

    //soldiersModule = std::move(soldierModule);
}



void setup()
{
    Serial.begin(115200);
    watch.begin(&Serial);

    beginLvglHelper();
    lv_png_init();
    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    randomSeed(millis());
    
    crypto::CryptoModule::init();

    if (!FFat.begin(true)) 
    {
        Serial.println("Failed to mount FFat!");
        return;
    }
    
    FFatHelper::removeFilesIncludeWords("share", "txt");
    FFatHelper::removeFilesStartingWith("log.txt.share");
    FFatHelper::removeFilesStartingWith("test.txt");

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
    receiveParametersPage->setOnTransferPage(transferFromReceiveParametersToDhPage);
    receiveParametersPage->setOnUploadLogs(transferFromMainToUploadLogsPage);

    FFatHelper::begin();

}


void loop()
{
    lv_task_handler();
    delay(5);

}