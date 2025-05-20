#pragma once

#include <string>
#include <vector>
#include <map>
#include "Soldier.h"

class CommanderModule : public Soldier
{
public:
    CommanderModule(const std::string& name,
                    const std::string& publicCert,
                    const std::string& privateKey,
                    const std::string& publicCA,
                    int soldierNumber);

    // Getters
    const std::vector<std::string>& getCurrentSoldiers() const;
    int getCurrentHeartRate() const;

    // Setters
    void setCurrentHeartRate(int heartRate);

    // Manage soldiers list
    void addSoldier(const std::string& soldierName);
    void removeSoldier(const std::string& soldierName);
    void clearSoldiers();

private:
    std::map<uint16_t, Soldier> _currentSoldiers;
    int _currentHeartRate;
    std::vector<std::string> _currentSoldiersNames;
};