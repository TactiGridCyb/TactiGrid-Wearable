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

    static lv_obj_t * createInfoBox(const char* initialText = "HELLO WORLD")
    {
        lv_obj_t * infoBox = lv_obj_create(lv_scr_act());
        lv_obj_set_size(infoBox, 180, 100);
        lv_obj_center(infoBox);

        lv_obj_set_style_bg_color(infoBox, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(infoBox, LV_OPA_80, 0);
        lv_obj_set_style_radius(infoBox, 6, 0);

        lv_obj_t * label = lv_label_create(infoBox);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_label_set_text(label, initialText);
        lv_obj_center(label);

        lv_obj_add_flag(infoBox, LV_OBJ_FLAG_HIDDEN);

        return infoBox;
    }

    static void restartInfoBoxFadeout(lv_obj_t * infoBox, uint32_t delay_ms, uint32_t fade_time_ms, const char* text, bool free = false)
    {
        lv_obj_t * label = lv_obj_get_child(infoBox, 0);
        if(label) 
        {
            lv_label_set_text(label, text);
        }
        
        lv_obj_clear_flag(infoBox, LV_OBJ_FLAG_HIDDEN);
        
        lv_obj_move_foreground(infoBox);
        
        lv_obj_set_style_opa(infoBox, LV_OPA_COVER, 0);

        lv_obj_fade_out(infoBox, fade_time_ms, delay_ms);

        if(free)
        {
            lv_obj_add_event_cb(infoBox, freeInfoBoxCB, LV_EVENT_READY, NULL);
        }
    }

    static void freeInfoBoxCB(lv_event_t * e)
    {
        lv_obj_t * infoBox = lv_event_get_target(e);
        if(infoBox == NULL)
        {
            return;
        }

        lv_obj_del(infoBox);
    }

};

