#pragma once

#include <string>
#include <vector>

class CommanderModule {
public:
    CommanderModule(const std::string& name,
                    const std::string& publicCert,
                    const std::string& privateKey,
                    int soldierNumber);

    // Getters
    const std::string& getName() const;
    const std::string& getPublicCert() const;
    const std::string& getPrivateKey() const;
    int getSoldierNumber() const;
    const std::vector<std::string>& getCurrentSoldiers() const;
    int getCurrentHeartRate() const;

    // Setters
    void setName(const std::string& name);
    void setPublicCert(const std::string& publicCert);
    void setPrivateKey(const std::string& privateKey);
    void setSoldierNumber(int soldierNumber);
    void setCurrentHeartRate(int heartRate);

    // Manage soldiers list
    void addSoldier(const std::string& soldierName);
    void removeSoldier(const std::string& soldierName);
    void clearSoldiers();

private:
    std::string _name;
    std::string _publicCert;
    std::string _privateKey;
    int _soldierNumber;
    std::vector<std::string> _currentSoldiers;
    int _currentHeartRate;
};