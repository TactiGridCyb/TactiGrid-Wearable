#pragma once

#include <string>

class Soldier {
public:
    Soldier(const std::string& name,
            const std::string& publicCert,
            const std::string& privateKey,
            const std::string& caPublicCert,
            int soldierNumber);

    // Getters
    const std::string& getName() const;
    const std::string& getPublicCert() const;
    const std::string& getPrivateKey() const;
    const std::string& getCAPublicCert() const;
    int getSoldierNumber() const;
    int getCurrentHeartRate() const;

    // Setters
    void setName(const std::string& name);
    void setPublicCert(const std::string& publicCert);
    void setPrivateKey(const std::string& privateKey);
    void setCAPublicCert(const std::string& caPublicCert);
    void setSoldierNumber(int soldierNumber);
    void setCurrentHeartRate(int heartRate);

private:
    std::string _name;
    std::string _publicCert;
    std::string _privateKey;
    std::string _caPublicCert;
    int _soldierNumber;
    int _currentHeartRate;
};