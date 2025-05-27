#include "CommandersMissionPage.h"
#include "lvgl.h"

CommandersMissionPage::CommandersMissionPage(std::shared_ptr<LoraModule> loraModule, const std::string& logFilePath)
    : loraModule(std::move(loraModule)), logFilePath(logFilePath) {}

void CommandersMissionPage::createPage() {
    // deleteExistingFile(logFilePath);

    lv_color_t ballColor = lv_color_hex(0xff0000);
    lv_color_t secondBallColor = lv_color_hex(0xff0000);

    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "TactiGrid Commander");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t* waitingLabel = lv_label_create(mainPage);
    lv_obj_center(waitingLabel);
    lv_label_set_text(waitingLabel, "Waiting For Initial Coords");
    lv_obj_set_style_text_color(waitingLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    if (loraModule) {
        loraModule->setupListening();
    }
}