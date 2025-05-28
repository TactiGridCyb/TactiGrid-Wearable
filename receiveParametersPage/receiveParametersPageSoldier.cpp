#include "receiveParametersPageSoldier.h"

receiveParametersPageSoldier::receiveParametersPageSoldier(std::unique_ptr<WifiModule> wifiModule)
{
    this->wifiModule = std::move(wifiModule);
    this->mainPage = lv_scr_act();

    this->
}

void receiveParametersPageSoldier::createPage()
{
    lv_obj_t *btn = lv_btn_create(this->mainPage);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -40);
    lv_obj_add_event_cb(btn, [](lv_event_t* e) {
        auto* self = static_cast<receiveParametersPageSoldier*>(lv_event_get_user_data(e));
        if (self) self->onSocketOpened(e);
    },
    LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_label = lv_label_create(btn);
    lv_label_set_text(btn_label, "Open Socket");
    lv_obj_center(btn_label);

    for (int i = 0; i < sizeof(this->statusLabels) / sizeof(this->statusLabels[0]); ++i) {
        this->statusLabels[i] = lv_label_create(this->mainPage);
        lv_label_set_text(this->statusLabels[i], "");
        lv_obj_align(this->statusLabels[i], LV_ALIGN_TOP_MID, 0, 60 + i * 30);
    }
}


void receiveParametersPageSoldier::destroy()
{
    lv_obj_t* mainPage = lv_scr_act();
    lv_obj_remove_style_all(mainPage);
    lv_obj_clean(mainPage);
}

void receiveParametersPageSoldier::onSocketOpened(lv_event_t* event)
{
    // TCP SSL
    JsonDocument parameters = this->wifiModule->receiveJSONTCP("127.0.0.1", 8743);
    

}

void receiveParametersPageSoldier::updateLabel(uint8_t index)
{
    lv_label_set_text(this->statusLabels[index], this->messages[index]);
}