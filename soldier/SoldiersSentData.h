#include <stdint.h>

#pragma once


enum SoldiersStatus
{
    REGULAR,
    COMPROMISED,
    DEAD,
    SOS
};

struct SoldiersSentData {
    float tileLat;
    float tileLon;
    float posLat;
    float posLon;
    int heartRate;
    uint16_t soldiersID;
    enum SoldiersStatus status;
};

