#include "../mainPage/CommandersMainPage.h"
#include "../receiveParametersPage/CommandersReceiveParametersPage.h"
#include "../missionPage/CommandersMissionPage.h"
#include <LoraModule.h>
#include <GPSModule.h>

const char* ssid = "default";
const char* password = "1357924680";

std::unique_ptr<CommandersReceiveParametersPage> commandersReceiveParametersPage;
std::unique_ptr<CommandersMainPage> commandersMainPage;
std::unique_ptr<CommandersMissionPage> commandersMissionPage;

std::unique_ptr<WifiModule> wifiModule;
std::shared_ptr<LoraModule> loraModule;
std::shared_ptr<GPSModule> gpsModule;


void transferFromMainToReceiveCoordsPage(std::unique_ptr<WifiModule> currentWifiModule)
{
    Serial.println("transferFromMainToSendCoordsPage");
    loraModule = std::make_shared<LoraModule>(433.5);
    gpsModule = std::make_shared<GPSModule>();

    loraModule->setup(true);

    commandersMissionPage = std::make_unique<CommandersMissionPage>(loraModule, std::move(currentWifiModule), gpsModule);
    commandersMissionPage->createPage();
}

void transferFromMainToUploadLogsPage(std::unique_ptr<WifiModule> currentWifiModule)
{

}

void transferFromReceiveParametersToMainPage(std::unique_ptr<WifiModule> currentWifiModule)
{
    Serial.println("transferFromReceiveParametersToMainPage");
    commandersMainPage = std::make_unique<CommandersMainPage>(std::move(currentWifiModule));
    commandersMainPage->createPage();

    commandersMainPage->setOnTransferReceiveCoordsPage(transferFromMainToReceiveCoordsPage);
    commandersMainPage->setOnTransferUploadLogsPage(transferFromMainToUploadLogsPage);
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
    
    commandersReceiveParametersPage = std::make_unique<CommandersReceiveParametersPage>(std::move(wifiModule));
    commandersReceiveParametersPage->createPage();
    commandersReceiveParametersPage->setOnTransferPage(transferFromReceiveParametersToMainPage);

}


void loop()
{
    lv_task_handler();
    delay(5);

}