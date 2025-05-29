#include "GPSModule.h"

GPSModule::GPSModule(float readInterval)
{
    this->readInterval = readInterval;
    this->lastCheck = millis();

    this->currentLat = 0.0f;
    this->currentLat = 0.0f;
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

float GPSModule::getLat() const
{
    return this->currentLat;
}

float GPSModule::getLon() const
{
    return this->currentLon;
}

void GPSModule::updateCoords()
{
    this->readGPSData();

    unsigned long currentTime = millis();

    if (currentTime - this->lastCheck < this->readInterval * 1000)
    {
        return;
    }

    this->lastCheck = currentTime;

    float lat = this->gpsInstance.location.isValid() ? this->gpsInstance.location.lat() : 0.0f;
    float lng = this->gpsInstance.location.isValid() ? this->gpsInstance.location.lng() : 0.0f;

    if(lat > 0.0f && lng > 0.0f)
    {
        this->currentLat = lat;
        this->currentLon = lng;
    }
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

inline GPSCoordTuple GPSModule::parseCoordinates(const String &message)
{
    float lat1 = 0, lon1 = 0, lat2 = 0, lon2 = 0;
    sscanf(message.c_str(), "%f,%f;%f,%f", &lat1, &lon1, &lat2, &lon2);
    return std::make_tuple(lat1, lon1, lat2, lon2);
}