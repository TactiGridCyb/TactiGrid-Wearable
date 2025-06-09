#include <CommandersMainPage.h>
#include <CommandersReceiveParametersPage.h>
#include <CommandersMissionPage.h>
#include <LoraModule.h>
#include <GPSModule.h>
#include <Commander.h>
#include <FHFModule.h>
#include <CommandersUploadLogPage.h>
#include <SoldiersMissionPage.h>
#include "../env.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

std::unique_ptr<CommandersReceiveParametersPage> commandersReceiveParametersPage;
std::unique_ptr<CommandersMainPage> commandersMainPage;
std::unique_ptr<CommandersMissionPage> commandersMissionPage;
std::unique_ptr<CommandersUploadLogPage> commandersUploadLogPage;
std::unique_ptr<SoldiersMissionPage> soldiersMissionPage;

std::unique_ptr<WifiModule> wifiModule;
std::shared_ptr<LoraModule> loraModule;
std::shared_ptr<GPSModule> gpsModule;
std::unique_ptr<FHFModule> fhfModule;

std::unique_ptr<Commander> commandersModule;

const std::string logFilePath = "/log.txt";

void transferFromSendCoordsToReceiveCoordsPage(std::shared_ptr<LoraModule> newLoraModule, 
    std::shared_ptr<GPSModule> newGPSModule, std::unique_ptr<FHFModule> newFHFModule,
    std::unique_ptr<Commander> commandersModule)
{
    
    
    commandersMissionPage = std::make_unique<CommandersMissionPage>(newLoraModule, 
    std::move(wifiModule), newGPSModule, std::move(newFHFModule), std::move(commandersModule), logFilePath);

    Serial.println("commandersMissionPage->createPage()");
    commandersMissionPage->createPage();

    Serial.println("commandersMissionPage->createPage() finished");
    soldiersMissionPage.reset();
}

void transferFromMissionCommanderToMissionSoldier(std::shared_ptr<LoraModule> newLoraModule,
     std::unique_ptr<WifiModule> newWifiModule,
     std::shared_ptr<GPSModule> newGPSModule, std::unique_ptr<FHFModule> newFhfModule,
     std::unique_ptr<Soldier> soldiersModule)
{
    commandersMissionPage.reset();
    
    Serial.println("transferFromMissionCommanderToMissionSoldier");

    wifiModule = std::move(newWifiModule);

    soldiersMissionPage = std::make_unique<SoldiersMissionPage>(newLoraModule,
     newGPSModule, std::move(newFhfModule), std::move(soldiersModule), true);

    Serial.println("std::make_unique<SoldiersMissionPage>");

    soldiersMissionPage->createPage();
    Serial.println("soldiersMissionPage->createPage()");

    UBaseType_t highWaterMark = uxTaskGetStackHighWaterMark(NULL);
    Serial.printf("[🧠 STACK] - Stack high water mark: %u words (~%u bytes)\n", 
                  highWaterMark, highWaterMark * sizeof(uint32_t));

    // soldiersMissionPage->setTransferFunction(transferFromSendCoordsToReceiveCoordsPage);
}

void transferFromMainToReceiveCoordsPage()
{
    Serial.println("transferFromMainToSendCoordsPage");
    loraModule = std::make_shared<LoraModule>(433.5);
    gpsModule = std::make_shared<GPSModule>();
    fhfModule = std::make_unique<FHFModule>(commandersModule->getFrequencies());


    loraModule->setup(true);

    commandersMissionPage = std::make_unique<CommandersMissionPage>(loraModule,
         std::move(wifiModule), gpsModule, std::move(fhfModule), std::move(commandersModule), logFilePath);
    commandersMissionPage->createPage();
    commandersMissionPage->setTransferFunction(transferFromMissionCommanderToMissionSoldier);
}

void transferFromMainToUploadLogsPage()
{
    commandersUploadLogPage = std::make_unique<CommandersUploadLogPage>(std::move(wifiModule), logFilePath);
    commandersUploadLogPage->createPage();
}

void transferFromReceiveParametersToMainPage(std::unique_ptr<WifiModule> currentWifiModule, std::unique_ptr<Commander> commanderModule)
{
    Serial.println("transferFromReceiveParametersToMainPage");

    wifiModule = std::move(currentWifiModule);

    commandersMainPage = std::make_unique<CommandersMainPage>();
    commandersMainPage->createPage();

    commandersMainPage->setOnTransferReceiveCoordsPage(transferFromMainToReceiveCoordsPage);
    commandersMainPage->setOnTransferUploadLogsPage(transferFromMainToUploadLogsPage);

    commandersModule = std::move(commanderModule);
}



void setup()
{
    Serial.begin(115200);
    watch.begin(&Serial);

    beginLvglHelper();
    lv_png_init();
    randomSeed(millis());

    crypto::CryptoModule::init();

    if (!FFat.begin(true)) 
    {
        Serial.println("Failed to mount FFat!");
        return;
    }
    else
    {
        Serial.println("Mounted FFat!");
    }
    
    FFatHelper::removeFilesIncludeWords("share", "txt");
    FFatHelper::removeFilesStartingWith("log.txt.share");
    FFatHelper::removeFilesStartingWith("test.txt");
    
    FFatHelper::deleteFile("/cert.txt");
    FFatHelper::deleteFile("/encLog.txt");
    FFatHelper::deleteFile("/CAcert.txt");
    FFatHelper::deleteFile("/encKey.txt");
    FFatHelper::deleteFile("/uploadPayload.txt");

    File log = FFat.open("/log.txt", FILE_WRITE);
    const char* logEx = R"JSON({"Interval":2000,"Mission":"68448016c3264abf41426988",
    "Data":[{"time_sent":"2025-06-09T08:37:13.816579Z","latitude":31.97087,
    "longitude":34.78566,"heartRate":78, "soldierId": "1"},{"time_sent":"2025-06-09T08:37:20.839632Z",
    "latitude":31.97087,"longitude":34.78568,"heartRate":100, "soldierId": "2"},
    {"time_sent":"2025-06-09T08:37:27.797917Z","latitude":31.97086,
    "longitude":34.78564,"heartRate":55, "soldierId": "1"},{"time_sent":"2025-06-09T08:37:34.799619Z",
    "latitude":31.97084,"longitude":34.78562,"heartRate":0, "soldierId": "2"}],
    "Events":[]})JSON";

    log.print(logEx);

    log.close();



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
    

    commandersReceiveParametersPage = std::make_unique<CommandersReceiveParametersPage>(std::move(wifiModule));
    commandersReceiveParametersPage->createPage();
    commandersReceiveParametersPage->setOnTransferPage(transferFromReceiveParametersToMainPage);

}


void loop()
{
    lv_task_handler();
    delay(5);


}