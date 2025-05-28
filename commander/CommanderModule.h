#pragma once

#include <string>
#include "mbedtls/x509_crt.h"
#include "CryptoModule.h"
#include "mbedtls/pk.h"
#include "PersonBase.h"
#include "../certModule/certModule.h"

class CommanderModule : public PersonBase<SoldierInfo>
{
public:
    CommanderModule(const std::string& name,
                    const mbedtls_x509_crt& publicCert,
                    const mbedtls_pk_context& privateKey,
                    const mbedtls_x509_crt& publicCA,
                    uint16_t soldierNumber, uint16_t intervalMS);

    int getCurrentHeartRate() const;
    void setCurrentHeartRate(int heartRate);

private:
    std::string name;
    uint16_t soldierNumber;
    uint16_t currentHeartRate;
    uint16_t intervalMS;

    crypto::Key256 GK;
    crypto::Key256 GMK;

    mbedtls_pk_context privateKey;
    mbedtls_x509_crt caCertificate;
    mbedtls_x509_crt ownCertificate;
};