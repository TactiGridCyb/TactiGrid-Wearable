#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <FFat.h>
#include "../wifi-connection/WifiModule.h"
#include "../LoraModule/LoraModule.h"
#include "../env.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASS;

std::unique_ptr<LoraModule> loraModule;
std::unique_ptr<WifiModule> wifiModule;

lv_timer_t * sendTimer;
int currentIndex = 1;

void clearMainPage()
{
    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_remove_style_all(mainPage);
    lv_obj_clean(mainPage);

}

void sendCoordinate(int index) {
    Serial.printf("sendCoordinate");
    String hello = "hello from COM4 " + index;
    loraModule->sendData(hello.c_str());
}

void sendTimerCallback(lv_timer_t *timer) {
    if(currentIndex % 9 != 0) {
  
      struct tm timeInfo;
      char timeStr[9];
  
      if(getLocalTime(&timeInfo)) {
          strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeInfo);
      } else {
          strcpy(timeStr, "00:00:00");
      }
      
      sendCoordinate(currentIndex);
      
                            
      currentIndex++;
    } else {
        lv_timer_del(timer);
        currentIndex = 0;
    }
}


lv_obj_t *mainLabel;

void init_main_menu()
{
    Serial.println("init_main_poc_page");
    clearMainPage();

    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "TactiGrid Soldier");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

}

void onRead(String data)
{
    lv_label_set_text(mainLabel, data.c_str());
    Serial.println("Received: " + data);


}

void setup() {
    Serial.begin(115200);
    watch.begin(&Serial);
    Serial.println("HELLO");
    
    
    if (!FFat.begin(true)) {
        Serial.println("Failed to mount FFat!");
        return;
    }


    beginLvglHelper();
    Serial.println("LVGL set!");

    // wifiModule = std::make_unique<WifiModule>(ssid, password);
    // wifiModule->connect(60000);

    loraModule = std::make_unique<LoraModule>(868.0);
    loraModule->setup(false);
    loraModule->setOnReadData(onRead);

    Serial.println("After LORA INIT");

    init_main_menu();

    loraModule->setupListening();
    Serial.println("End of setup");

    
}

void loop()
{
    lv_timer_handler();
    delay(5);
    
    loraModule->readData();
}