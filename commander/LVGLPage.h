#pragma once

#include <lvgl.h>

#define LV_HOR_RES_MAX 240
#define LV_VER_RES_MAX 240

class LVGLPage {
    public:
    virtual ~LVGLPage(){}
    virtual void createPage() = 0;
    virtual void destroyPage()
    {
        lv_obj_t * mainPage = lv_scr_act();
        lv_obj_remove_style_all(mainPage);
        lv_obj_clean(mainPage);
    }

    void transferToAnotherPage(LVGLPage* newPage)
    {
        this->destroyPage();
        newPage->createPage();
    }

};

