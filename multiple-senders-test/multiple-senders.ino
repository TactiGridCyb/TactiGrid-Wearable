#include <LilyGoLib.h>
#include <LV_Helper.h>
#include <FFat.h>
#include "../wifi-connection/WifiModule.h"
#include "../LoraModule/LoraModule.h"

const char* ssid = "default";
const char* password = "1357924680";

std::unique_ptr<LoraModule> loraModule;
std::unique_ptr<WifiModule> wifiModule;

lv_timer_t * sendTimer;
int currentIndex = 1;
bool initialTransmission = false;

void clearMainPage()
{
    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_remove_style_all(mainPage);
    lv_obj_clean(mainPage);

}

uint8_t sendCoordinate(int index) {
    Serial.println("sendCoordinate");
    String hello = "hello from COM4";
    return loraModule->sendData(hello.c_str());
}

void sendTimerCallback(lv_timer_t *timer) {
    if (currentIndex > 50) {
        lv_timer_del(timer);
        Serial.println("All packets sent, timer deleted.");
        return;
    }

    if (loraModule->isChannelFree()) {
        uint8_t status = sendCoordinate(currentIndex);
        if (status == RADIOLIB_ERR_NONE) {
            Serial.printf("Packet %d queued for TX\n", currentIndex);
            currentIndex++;
        } else {
            Serial.printf("sendData() error %d\n", status);
        }
    } else {
        Serial.println("Channel busy, will retry on next tick");
        delay(random(200, 500));
    }
}

void init_send_coords_page(lv_event_t * event)
{
  Serial.println("init_send_coords_page");
  clearMainPage();

  lv_obj_t * mainPage = lv_scr_act();
  lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

  lv_obj_t *mainLabel = lv_label_create(mainPage);
  lv_label_set_text(mainLabel, "TactiGrid Soldier");
  lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);


  sendTimer = lv_timer_create(sendTimerCallback, 1500, NULL);

}

void init_main_menu()
{
    Serial.println("init_main_poc_page");
    clearMainPage();

    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "TactiGrid Soldier");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *sendCoordsBtn = lv_btn_create(mainPage);
    lv_obj_center(sendCoordsBtn);
    lv_obj_set_style_bg_color(sendCoordsBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(sendCoordsBtn, init_send_coords_page, LV_EVENT_CLICKED, NULL);

    lv_obj_t *sendCoordsLabel = lv_label_create(sendCoordsBtn);
    lv_label_set_text(sendCoordsLabel, "Send Coords");
    lv_obj_center(sendCoordsLabel);
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


    loraModule = std::make_unique<LoraModule>(868.0);
    loraModule->setup(true);
    
    loraModule->sendData("hello world first!");

    Serial.println("After LORA INIT");

    init_main_menu();

    Serial.println("End of setup");
}

void loop()
{
    lv_timer_handler();
    delay(5);
    
    loraModule->cleanUpTransmissions();
}