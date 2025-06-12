#pragma once

#include <lvgl.h>
#include <LilyGoLib.h>

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

    static void generateNearbyCoordinatesFromTile(int tileX, int tileY, int zoom,
                                                             float& outLat, float& outLon) {
        float n = pow(2.0f, zoom);

        float lonLeft = tileX / n * 360.0f - 180.0f;
        float lonRight = (tileX + 1) / n * 360.0f - 180.0f;

        float latTop = atan(sinh(M_PI * (1 - 2 * tileY / n))) * 180.0f / M_PI;
        float latBottom = atan(sinh(M_PI * (1 - 2 * (tileY + 1) / n))) * 180.0f / M_PI;

        float randLat = static_cast<float>(rand()) / RAND_MAX;
        float randLon = static_cast<float>(rand()) / RAND_MAX;

        outLat = latBottom + randLat * (latTop - latBottom);
        outLon = lonLeft + randLon * (lonRight - lonLeft);
    }

    static uint8_t generateHeartRate() 
    {
        return rand() % 101 + 50;
    }

};

