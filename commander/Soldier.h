#pragma once

#include <string>
#include <vector>
#include "mbedtls/x509_crt.h"
#include "CryptoModule.h"
#include "mbedtls/pk.h"
#include "PersonBase.h"

class Soldier : public PersonBase<CommanderInfo> {
public:
    Soldier(const std::string& name,
            const mbedtls_x509_crt& publicCert,
            const mbedtls_pk_context& privateKey,
            const mbedtls_x509_crt& caPublicCert,
            uint16_t soldierNumber,
            uint16_t intervalMS);

    const std::string& getName() const;
    const mbedtls_x509_crt& getPublicCert() const;
    const mbedtls_x509_crt& getCAPublicCert() const;
    uint16_t getSoldierNumber() const;
    uint16_t getCurrentHeartRate() const;

    void setName(const std::string& name);
    void setPublicCert(const std::string& publicCert);
    void setPrivateKey(const std::string& privateKey);
    void setCAPublicCert(const std::string& caPublicCert);
    void setSoldierNumber(uint16_t soldierNumber);
    void setCurrentHeartRate(uint16_t heartRate);

    const std::vector<float>& getFrequencies() const;
    void appendFrequencies(const std::vector<float>& freqs);

private:
    std::string name;
    uint16_t soldierNumber;
    uint16_t currentHeartRate;
    uint16_t intervalMS;

    std::vector<float> frequencies;

    crypto::Key256 GK;
    crypto::Key256 GMK;

    mbedtls_pk_context privateKey;
    mbedtls_x509_crt caCertificate;
    mbedtls_x509_crt ownCertificate;
};