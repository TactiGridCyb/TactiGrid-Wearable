#include "receiveParametersPage.h"

receiveParametersPage::receiveParametersPage(std::shared_ptr<WifiModule> wifiModule)
{
    this->wifiModule = std::move(wifiModule);
}


void receiveParametersPage::destroy()
{
    lv_obj_t* mainPage = lv_scr_act();
    lv_obj_remove_style_all(mainPage);
    lv_obj_clean(mainPage);
}