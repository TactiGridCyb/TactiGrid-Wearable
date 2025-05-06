#pragma once

#include <TinyGPSPlus.h>
#include <LilyGoLib.h>
#include <tuple>

class GPSModule{
    private:
    TinyGPSPlus gpsInstance;
    float readInterval; // Seconds
    unsigned long lastCheck;

    public:
    GPSModule(float readInterval = 1.2f);

    void readGPSData();
    std::tuple<float, float> getCurrentCoords();
};