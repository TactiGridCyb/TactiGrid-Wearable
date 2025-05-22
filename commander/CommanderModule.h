#pragma once

#include <string>
#include <vector>
#include <map>
#include "Soldier.h"
#include "../certModule/certModule.h"
#include <array>

class CommanderModule : public Soldier
{
public:
    CommanderModule(const std::string& name,
                    const std::string& publicCert,
                    const std::string& privateKey,
                    const std::string& publicCA,
                    int soldierNumber);

    const std::vector<std::string>& getCurrentSoldiers() const;
    int getCurrentHeartRate() const;

    void setCurrentHeartRate(int heartRate);

    void addSoldier(const std::string& soldierName);
    void removeSoldier(const std::string& soldierName);
    void clearSoldiers();

private:
    std::map<uint16_t, Soldier> currentSoldiers;
    int currentHeartRate;
    std::vector<certModule> currentSoldiersCerts;
};