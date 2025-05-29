#include "SoldiersMainPage.h"

SoldiersMainPage::SoldiersMainPage(std::unique_ptr<WifiModule> wifiModule)
{
    this->wifiModule = std::move(wifiModule);

    this->mainPage = lv_scr_act();
}

void SoldiersMainPage::createPage()
{
    Serial.println("SoldiersMainPage::createPage");

    lv_obj_set_style_bg_color(this->mainPage, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(this->mainPage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    this->wifiModule->downloadFile(this->helmentDownloadLink, "/helmet.png");
    this->showHelmet();

    lv_obj_t *mainLabel = lv_label_create(this->mainPage);
    lv_label_set_text(mainLabel, "TactiGrid Soldier");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *sendCoordsBtn = lv_btn_create(this->mainPage);
    lv_obj_center(sendCoordsBtn);
    lv_obj_set_style_bg_color(sendCoordsBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(sendCoordsBtn, [](lv_event_t* e) 
    {
        auto * self = static_cast<SoldiersMainPage*>(lv_event_get_user_data(e));
        self->destroyPage();
        delay(10);
        self->onTransferPage(std::move(self->wifiModule));
    },
    LV_EVENT_CLICKED, this);

    lv_obj_t *sendCoordsLabel = lv_label_create(sendCoordsBtn);
    lv_label_set_text(sendCoordsLabel, "Send Coords");
    lv_obj_center(sendCoordsLabel);
}

void SoldiersMainPage::showHelmet()
{

    lv_obj_t * helmetIMG = lv_img_create(this->mainPage);

    lv_img_set_src(helmetIMG, "A:/helmet.png");
    lv_obj_align(helmetIMG, LV_ALIGN_BOTTOM_MID, 0, 70);

    lv_img_set_zoom(helmetIMG, 64);

}

void SoldiersMainPage::setOnTransferPage(std::function<void(std::unique_ptr<WifiModule>)> cb)
{
    this->onTransferPage = std::move(cb);
}