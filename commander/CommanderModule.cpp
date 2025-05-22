
#include "CommanderModule.h"
#include <algorithm>

CommanderModule::CommanderModule(const std::string& name,
                               const std::string& publicCert,
                               const std::string& privateKey,
                               const std::string& publicCA,
                               int soldierNumber)
    : Soldier(name, publicCert, privateKey, publicCA, soldierNumber),
      currentHeartRate(0)
{
}

const std::vector<std::string>& CommanderModule::getCurrentSoldiers() const {
    return currentSoldiersCerts;
}

int CommanderModule::getCurrentHeartRate() const {
    return currentHeartRate;
}

void CommanderModule::setCurrentHeartRate(int heartRate) {
    currentHeartRate = heartRate;
}

void CommanderModule::addSoldier(const std::string& soldierName) {
    // Add soldier name if not already present
    if (std::find(currentSoldiersCerts.begin(), currentSoldiersCerts.end(), soldierName) == currentSoldiersCerts.end()) {
        currentSoldiersCerts.push_back(soldierName);
    }
}

void CommanderModule::removeSoldier(const std::string& soldierName) {
    auto it = std::find(currentSoldiersCerts.begin(), currentSoldiersCerts.end(), soldierName);
    if (it != currentSoldiersCerts.end()) {
        currentSoldiersCerts.erase(it);
    }
}

void CommanderModule::clearSoldiers() {
    currentSoldiersCerts.clear();
}