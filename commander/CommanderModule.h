#pragma once

#include "Soldier.h"
#include <string>
#include <vector>
#include <map>
#include <array>
#include "../certModule/certModule.h"

class CommanderModule : public Soldier
{
public:
    CommanderModule(const std::string& name,
                    const mbedtls_x509_crt& publicCert,
                    const mbedtls_pk_context& privateKey,
                    const mbedtls_x509_crt& publicCA,
                    uint16_t soldierNumber);

    const std::vector<mbedtls_x509_crt>& getCurrentSoldiers() const;
    int getCurrentHeartRate() const;

    void setCurrentHeartRate(int heartRate);

    void addSoldier(mbedtls_x509_crt& soldierCert);
    void removeSoldier(mbedtls_x509_crt& soldierCert);
    void clearSoldiers();

private:
    std::map<uint16_t, Soldier> currentSoldiers;
    int currentHeartRate;
    std::vector<mbedtls_x509_crt> currentSoldiersCerts;
};