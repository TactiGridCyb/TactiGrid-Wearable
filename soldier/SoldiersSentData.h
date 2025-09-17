#pragma once
#include <stdint.h>
#include <lvgl.h>

struct SoldiersSentData {
    float tileLat;
    float tileLon;
    float posLat;
    float posLon;
    uint16_t heartRate;
    uint16_t soldiersID;
};

inline lv_color_t getColorFromHeartRate(uint16_t hr) {
    if (hr <= 0) return lv_color_black();

    const int min_hr = 40;
    const int max_hr = 170;

    if (hr < min_hr)
    {
        hr = min_hr;
    }
    else if (hr > max_hr)
    {
        hr = max_hr;
    }

    int hue = 120 * (hr - min_hr) / (max_hr - min_hr);

    return lv_color_hsv_to_rgb(hue, 100, 100);
}

