#include "MainSoldiersPage.h"
#include "SoldiersInMissionPage.h"

MainSoldiersPage::MainSoldiersPage(lv_obj_t* parent)
{
    this->parent = parent;
}

void MainSoldiersPage::createPage()
{
    lv_obj_set_style_bg_color(this->parent, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(this->parent, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *mainLabel = lv_label_create(this->parent);
    lv_label_set_text(mainLabel, "TactiGrid Soldier");
    lv_obj_align(mainLabel, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_color(mainLabel, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *sendCoordsBtn = lv_btn_create(this->parent);
    lv_obj_center(sendCoordsBtn);
    lv_obj_set_style_bg_color(sendCoordsBtn, lv_color_hex(0x346eeb), LV_STATE_DEFAULT);
    lv_obj_add_event_cb(sendCoordsBtn, this->openSendCoordsPage, LV_EVENT_CLICKED, NULL);

    lv_obj_t *sendCoordsLabel = lv_label_create(sendCoordsBtn);
    lv_label_set_text(sendCoordsLabel, "Send Coords");
    lv_obj_center(sendCoordsLabel);
}

void MainSoldiersPage::openSendCoordsPage(lv_event_t* event)
{
    MainSoldiersPage* currentPage = static_cast<MainSoldiersPage*>(lv_event_get_user_data(event));

    SoldiersInMissionPage soldiersInMissionPage;

    LVGLPage::closeAndOpenPage(currentPage, &soldiersInMissionPage);
}