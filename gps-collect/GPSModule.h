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
    std::tuple<float, float, float> getCurrentCoords();
    TinyGPSDate getGPSDate();
    TinyGPSTime getGPSTime();
    TinyGPSHDOP getGPSHDOP();
    TinyGPSAltitude getGPSAltitude();
    TinyGPSInteger getGPSSatellites();
    TinyGPSSpeed getGPSSpeed();
    
};