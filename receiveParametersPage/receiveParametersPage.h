#include <lvgl.h>
#include <LV_Helper.h>
#include "../wifi-connection/WifiModule.h"

class receiveParametersPage{
    public:
    void destroy();

    receiveParametersPage();

    private:
    std::unique_ptr<WifiModule> wifiModule;

    lv_obj_t* btnReceiveParameters;
    lv_obj_t* label;
};