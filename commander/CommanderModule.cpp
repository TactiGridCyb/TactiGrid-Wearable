
#include "CommanderModule.h"
#include <algorithm>

CommanderModule::CommanderModule(const std::string& name,
                               const std::string& publicCert,
                               const std::string& privateKey,
                               const std::string& publicCA,
                               int soldierNumber)
    : Soldier(name, publicCert, privateKey, publicCA, soldierNumber),
      _currentHeartRate(0)
{
}

const std::vector<std::string>& CommanderModule::getCurrentSoldiers() const {
    return _currentSoldiersNames;
}

int CommanderModule::getCurrentHeartRate() const {
    return _currentHeartRate;
}

void CommanderModule::setCurrentHeartRate(int heartRate) {
    _currentHeartRate = heartRate;
}

void CommanderModule::addSoldier(const std::string& soldierName) {
    // Add soldier name if not already present
    if (std::find(_currentSoldiersNames.begin(), _currentSoldiersNames.end(), soldierName) == _currentSoldiersNames.end()) {
        _currentSoldiersNames.push_back(soldierName);
    }
}

void CommanderModule::removeSoldier(const std::string& soldierName) {
    auto it = std::find(_currentSoldiersNames.begin(), _currentSoldiersNames.end(), soldierName);
    if (it != _currentSoldiersNames.end()) {
        _currentSoldiersNames.erase(it);
    }
}

void CommanderModule::clearSoldiers() {
    _currentSoldiersNames.clear();
}