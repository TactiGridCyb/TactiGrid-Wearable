#include "GPSModule.h"

GPSModule::GPSModule(float readInterval = 1.2f)
{
    this->readInterval = readInterval;
    this->lastCheck = millis();
}

void GPSModule::readGPSData()
{
    while (GPSSerial.available()) {
        int r = GPSSerial.read();
        this->gpsInstance.encode(r);
    }
}

bool GPSModule::isZero(float value) {
    const float epsilon = 1e-6f;
    return fabsf(value) < epsilon;
}

std::tuple<float, float> GPSModule::getCurrentCoords()
{
    unsigned long currentTime = millis();

    if (currentTime - this->lastCheck < this->readInterval * 1000)
    {
        return std::make_tuple(0.0f, 0.0f);
    }
    
    this->lastCheck = currentTime;

    float lat = gpsInstance.location.isValid() ? gpsInstance.location.lat() : 0.0f;
    float lng = gpsInstance.location.isValid() ? gpsInstance.location.lng() : 0.0f;

    if (isZero(lat) || isZero(lng)) {
        return std::make_tuple(0.0f, 0.0f);
    }

    return std::make_tuple(lat, lng);
}