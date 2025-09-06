#pragma once

#include <string>
#include <vector>
#include "mbedtls/x509_crt.h"
#include "../encryption/CryptoModule.h"
#include "mbedtls/pk.h"
#include "PersonBase.h"

class Soldier : public PersonBase {
public:
    Soldier(const std::string& name,
            const mbedtls_x509_crt& publicCert,
            const mbedtls_pk_context& privateKey,
            const mbedtls_x509_crt& caPublicCert,
            uint8_t soldierNumber,
            uint16_t intervalMS);

    const std::string& getName() const;
    const mbedtls_x509_crt& getPublicCert() const;
    const mbedtls_x509_crt& getCAPublicCert() const;
    const mbedtls_pk_context& getPrivateKey() const;
    uint8_t getSoldierNumber() const;
    uint16_t getCurrentHeartRate() const;
    
    const crypto::Key256& getGMK() const 
    {
        return GMK;
    }

    const crypto::Key256& getGK() const 
    {
        return GK;
    }

    const uint16_t getIntervalMS() const
    {
        return intervalMS;
    }

    void setName(const std::string& name);
    void setPublicCert(const std::string& publicCert);
    void setPrivateKey(const std::string& privateKey);
    void setCAPublicCert(const std::string& caPublicCert);
    void setSoldierNumber(uint8_t soldierNumber);
    void setCurrentHeartRate(uint16_t heartRate);

    const std::vector<float>& getFrequencies() const;
    void appendFrequencies(const std::vector<float>& freqs);

    void setGMK(const crypto::Key256& gmk);
    void setGK(const crypto::Key256& gk);

    void clear();

    

private:
    std::string name;
    uint8_t soldierNumber;
    uint16_t currentHeartRate;
    uint16_t intervalMS;

    std::vector<float> frequencies;

    crypto::Key256 GK;
    crypto::Key256 GMK = []() {
        crypto::Key256 key{};
        const char* raw = "0123456789abcdef0123456789abcdef";
        std::memcpy(key.data(), raw, 32);
        return key;
    }();

    mbedtls_pk_context privateKey;
    mbedtls_x509_crt caCertificate;
    mbedtls_x509_crt ownCertificate;
};