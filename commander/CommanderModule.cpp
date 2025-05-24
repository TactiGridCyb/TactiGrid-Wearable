
#include "CommanderModule.h"
#include <algorithm>

CommanderModule::CommanderModule(const std::string& name,
                               const mbedtls_x509_crt& publicCert,
                               const mbedtls_pk_context& privateKey,
                               const mbedtls_x509_crt& publicCA,
                               uint16_t soldierNumber)
    : Soldier(name, publicCert, privateKey, publicCA, soldierNumber),
      currentHeartRate(0)
{
}

const std::vector<mbedtls_x509_crt>& CommanderModule::getCurrentSoldiers() const {
    return currentSoldiersCerts;
}

int CommanderModule::getCurrentHeartRate() const {
    return currentHeartRate;
}

void CommanderModule::setCurrentHeartRate(int heartRate) {
    currentHeartRate = heartRate;
}

void CommanderModule::addSoldier(mbedtls_x509_crt& soldierCert) {
    auto it = std::find_if(
        currentSoldiersCerts.begin(),
        currentSoldiersCerts.end(),
        [&soldierCert](const mbedtls_x509_crt& cert) { return &cert == &soldierCert; }
    );
    if (it == currentSoldiersCerts.end()) {
        currentSoldiersCerts.push_back(soldierCert);
    }
}

void CommanderModule::removeSoldier(mbedtls_x509_crt& soldierCert) {
    auto it = std::find_if(
        currentSoldiersCerts.begin(),
        currentSoldiersCerts.end(),
        [&soldierCert](const mbedtls_x509_crt& cert) { return &cert == &soldierCert; }
    );
    if (it != currentSoldiersCerts.end()) {
        currentSoldiersCerts.erase(it);
    }
}

void CommanderModule::clearSoldiers() {
    currentSoldiersCerts.clear();
}