#include "CommandersMainPage.h"

CommandersMainPage::CommandersMainPage(std::unique_ptr<WifiModule> wifiModule)
{

    std::string logFilePath = "/spiffs/log.txt";

    this->mainPage = lv_scr_act();


}

void CommandersMainPage::createPage()
{
    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_set_style_bg_color(mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);


    lv_obj_t *mainLabel = lv_label_create(mainPage);
    lv_label_set_text(mainLabel, "Main Menu");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *cont = lv_obj_create(lv_scr_act());
    lv_obj_set_size(cont, 240, 60);
    lv_obj_center(cont);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_flex_flow(cont, LV_FLEX_FLOW_ROW, 0);
    lv_obj_set_size(cont, lv_disp_get_hor_res(NULL), 60);

    lv_obj_set_layout(cont, LV_LAYOUT_FLEX);

    lv_obj_t *receiveCoordsBtn = lv_btn_create(cont);
    lv_obj_center(receiveCoordsBtn);
    lv_obj_set_style_bg_color(receiveCoordsBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_set_style_flex_grow(receiveCoordsBtn, 1, 0);
    lv_obj_add_event_cb(receiveCoordsBtn, [](lv_event_t* e) {
        auto* self = static_cast<CommandersMainPage*>(lv_event_get_user_data(e));
        if (self && self->onTransferReceiveCoordsPage)
        {
            self->destroyPage();
            delay(10);
            self->onTransferReceiveCoordsPage(std::move(self->wifiModule));
        }
    }, LV_EVENT_CLICKED, this);

    lv_obj_t *receiveCoordsLabel = lv_label_create(receiveCoordsBtn);
    lv_label_set_text(receiveCoordsLabel, "Receive Pos");
    lv_obj_center(receiveCoordsLabel);

    lv_obj_t *uploadLogsBtn = lv_btn_create(cont);
    lv_obj_center(uploadLogsBtn);
    lv_obj_set_style_flex_grow(uploadLogsBtn, 1, 0);
    lv_obj_set_style_bg_color(uploadLogsBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(uploadLogsBtn, [](lv_event_t* e) {
        auto* self = static_cast<CommandersMainPage*>(lv_event_get_user_data(e));
        if (self && self->onTransferUploadLogsPage)
        {
            self->destroyPage();
            delay(10);
            self->onTransferUploadLogsPage(std::move(self->wifiModule));
        }
    }, LV_EVENT_CLICKED, this);

    lv_obj_t *uploadLogsLabel = lv_label_create(uploadLogsBtn);
    lv_label_set_text(uploadLogsLabel, "Upload Logs");
    lv_obj_center(uploadLogsLabel);
}

void CommandersMainPage::setOnTransferReceiveCoordsPage(std::function<void(std::unique_ptr<WifiModule>)> cb)
{
    this->onTransferReceiveCoordsPage = std::move(cb);
}

void CommandersMainPage::setOnTransferUploadLogsPage(std::function<void(std::unique_ptr<WifiModule>)> cb)
{
    this->onTransferUploadLogsPage = std::move(cb);
}