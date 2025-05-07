#include "GPSModule.h"

GPSModule::GPSModule(float readInterval)
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

TinyGPSDate GPSModule::getGPSDate()
{
    return this->gpsInstance.date;
}

TinyGPSTime GPSModule::getGPSTime()
{
    return this->gpsInstance.time;
}

TinyGPSHDOP GPSModule::getGPSHDOP()
{
    return this->gpsInstance.hdop;
}

std::tuple<float, float, float> GPSModule::getCurrentCoords()
{
    unsigned long currentTime = millis();

    if (currentTime - this->lastCheck < this->readInterval * 1000)
    {
        return std::make_tuple(-1.0f, -1.0f, currentTime - this->lastCheck);
    }

    this->lastCheck = currentTime;

    float lat = this->gpsInstance.location.isValid() ? this->gpsInstance.location.lat() : 0.0f;
    float lng = this->gpsInstance.location.isValid() ? this->gpsInstance.location.lng() : 0.0f;

    return std::make_tuple(lat, lng, currentTime - this->lastCheck);
}

TinyGPSAltitude GPSModule::getGPSAltitude()
{
    return this->gpsInstance.altitude;
}

TinyGPSInteger GPSModule::getGPSSatellites()
{
    return this->gpsInstance.satellites;
}

TinyGPSSpeed GPSModule::getGPSSpeed()
{
    return this->gpsInstance.speed;
}