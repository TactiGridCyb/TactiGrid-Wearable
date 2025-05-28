#pragma once

#include <lvgl.h>

class LVGLPage {
    public:
    virtual ~LVGLPage(){}
    virtual void createPage() = 0;
    virtual void destroyPage()
    {
        lv_obj_t * mainPage = lv_scr_act();
        lv_obj_remove_style_all(mainPage);
        lv_obj_clean(mainPage);
        lv_obj_del(mainPage);
    }

    void transferToAnotherPage(LVGLPage* newPage)
    {
        this->destroyPage();
        newPage->createPage();
    }
};

