#include <stdint.h>

#pragma once

struct SoldiersSentData {
    float tileLat;
    float tileLon;
    float posLat;
    float posLon;
    int heartRate;
    uint16_t soldiersID;
    SoldiersStatus status;
};

enum SoldiersStatus
{
    REGULAR,
    COMPROMISED,
    DEAD,
    SOS
};