#include <CommandersMainPage.h>
#include <CommandersReceiveParametersPage.h>
#include <CommandersMissionPage.h>
#include <LoraModule.h>
#include <GPSModule.h>
#include <Commander.h>
#include <FHFModule.h>
#include <CommandersUploadLogPage.h>
#include "../env.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

std::unique_ptr<CommandersReceiveParametersPage> commandersReceiveParametersPage;
std::unique_ptr<CommandersMainPage> commandersMainPage;
std::unique_ptr<CommandersMissionPage> commandersMissionPage;
std::unique_ptr<CommandersUploadLogPage> commandersUploadLogPage;

std::unique_ptr<WifiModule> wifiModule;
std::shared_ptr<LoraModule> loraModule;
std::shared_ptr<GPSModule> gpsModule;
std::unique_ptr<FHFModule> fhfModule;

std::unique_ptr<Commander> commandersModule;

const std::string logFilePath = "/log.txt";

void transferFromMainToReceiveCoordsPage(std::unique_ptr<WifiModule> currentWifiModule)
{
    Serial.println("transferFromMainToSendCoordsPage");
    loraModule = std::make_shared<LoraModule>(433.5);
    gpsModule = std::make_shared<GPSModule>();
    fhfModule = std::make_unique<FHFModule>(commandersModule->getFrequencies());

    loraModule->setup(true);

    commandersMissionPage = std::make_unique<CommandersMissionPage>(loraModule,
         std::move(currentWifiModule), gpsModule, std::move(fhfModule), std::move(commandersModule), logFilePath);
    commandersMissionPage->createPage();
}

void transferFromMainToUploadLogsPage(std::unique_ptr<WifiModule> currentWifiModule)
{
    commandersUploadLogPage = std::make_unique<CommandersUploadLogPage>(std::move(currentWifiModule), logFilePath);
    commandersUploadLogPage->createPage();
}

void transferFromReceiveParametersToMainPage(std::unique_ptr<WifiModule> currentWifiModule, std::unique_ptr<Commander> commanderModule)
{
    Serial.println("transferFromReceiveParametersToMainPage");
    commandersMainPage = std::make_unique<CommandersMainPage>(std::move(currentWifiModule));
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

    if (!FFat.begin(true)) 
    {
        Serial.println("Failed to mount FFat!");
        return;
    }
    else
    {
        Serial.println("Mounted FFat!");
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
    
    commandersReceiveParametersPage = std::make_unique<CommandersReceiveParametersPage>(std::move(wifiModule));
    commandersReceiveParametersPage->createPage();
    commandersReceiveParametersPage->setOnTransferPage(transferFromReceiveParametersToMainPage);

}


void loop()
{
    lv_task_handler();
    delay(5);

}