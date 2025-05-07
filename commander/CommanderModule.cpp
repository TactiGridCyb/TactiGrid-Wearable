#include "CommanderModule.h"
#include <algorithm>

CommanderModule::CommanderModule(const std::string& name,
                               const std::string& publicCert,
                               const std::string& privateKey,
                               int soldierNumber)
    : _name(name),
      _publicCert(publicCert),
      _privateKey(privateKey),
      _soldierNumber(soldierNumber),
      _currentHeartRate(0)
{}

const std::string& CommanderModule::getName() const {
    return _name;
}

const std::string& CommanderModule::getPublicCert() const {
    return _publicCert;
}

const std::string& CommanderModule::getPrivateKey() const {
    return _privateKey;
}

int CommanderModule::getSoldierNumber() const {
    return _soldierNumber;
}

const std::vector<std::string>& CommanderModule::getCurrentSoldiers() const {
    return _currentSoldiers;
}

int CommanderModule::getCurrentHeartRate() const {
    return _currentHeartRate;
}

void CommanderModule::setName(const std::string& name) {
    _name = name;
}

void CommanderModule::setPublicCert(const std::string& publicCert) {
    _publicCert = publicCert;
}

void CommanderModule::setPrivateKey(const std::string& privateKey) {
    _privateKey = privateKey;
}

void CommanderModule::setSoldierNumber(int soldierNumber) {
    _soldierNumber = soldierNumber;
}

void CommanderModule::setCurrentHeartRate(int heartRate) {
    _currentHeartRate = heartRate;
}

void CommanderModule::addSoldier(const std::string& soldierName) {
    if (std::find(_currentSoldiers.begin(), _currentSoldiers.end(), soldierName) == _currentSoldiers.end()) {
        _currentSoldiers.push_back(soldierName);
    }
}

void CommanderModule::removeSoldier(const std::string& soldierName) {
    auto it = std::remove(_currentSoldiers.begin(), _currentSoldiers.end(), soldierName);
    if (it != _currentSoldiers.end()) {
        _currentSoldiers.erase(it, _currentSoldiers.end());
    }
}

void CommanderModule::clearSoldiers() {
    _currentSoldiers.clear();
}