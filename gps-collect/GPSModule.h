#pragma once

#include <TinyGPSPlus.h>
#include <LilyGoLib.h>
#include <tuple>

class GPSModule{
    private:
    TinyGPSPlus gpsInstance;
    float readInterval; // Seconds
    unsigned long lastCheck;
    float currentLat;
    float currentLon;

    public:
    GPSModule(float readInterval = 1.2f);

    void readGPSData();
    void updateCoords();

    float getLat() const;
    float getLon() const;

    TinyGPSDate getGPSDate();
    TinyGPSTime getGPSTime();
    TinyGPSHDOP getGPSHDOP();
    TinyGPSAltitude getGPSAltitude();
    TinyGPSInteger getGPSSatellites();
    TinyGPSSpeed getGPSSpeed();
    
};