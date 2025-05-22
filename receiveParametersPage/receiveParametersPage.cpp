#include "receiveParametersPage.h"

receiveParametersPage::receiveParametersPage()
{
    beginLvglHelper();

    btnReceiveParameters = lv_btn_create(lv_scr_act());
    lv_obj_set_size(btnReceiveParameters, 200, 50);
    lv_obj_align(btnReceiveParameters, LV_ALIGN_CENTER, 0, 0);

    label = lv_label_create(btnReceiveParameters);
    lv_label_set_text(label, "Receive Parameters");
    lv_obj_center(label);
}


void receiveParametersPage::destroy()
{
    lv_obj_t * mainPage = lv_scr_act();
    lv_obj_remove_style_all(mainPage);
    lv_obj_clean(mainPage);

    btnReceiveParameters = nullptr;
    label = nullptr;

}