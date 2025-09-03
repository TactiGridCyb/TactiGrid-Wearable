#pragma once

#include <string>
#include <vector>
#include "mbedtls/x509_crt.h"
#include "CryptoModule.h"
#include "mbedtls/pk.h"
#include "PersonBase.h"
#include <stdexcept>
#include <cstring>
#include "../certModule/certModule.h"

struct SwitchGMK{
    uint8_t msgID;
    uint8_t soldiersID;
    std::vector<uint8_t> encryptedGMK;
};

struct SwitchCommander{
    uint8_t msgID;
    uint8_t soldiersID;
    uint16_t shamirPartLength;
    std::vector<std::pair<uint8_t,uint8_t>> shamirPart;
    uint8_t compromisedSoldiersLength;
    std::vector<uint8_t> compromisedSoldiers;
    uint8_t missingSoldiersLength;
    std::vector<uint8_t> missingSoldiers;
    uint8_t soldiersCoordsLength;
    std::vector<std::tuple<float,float>> soldiersCoords;
};

struct requestShamir{
    uint8_t msgID;
    uint8_t soldiersID;
};

struct sendShamir{
    uint8_t msgID;
    uint8_t soldiersID;
    std::vector<std::pair<uint8_t,uint8_t>> shamirPart;
};

struct SkipCommander{
    uint8_t msgID;
    uint8_t commandersID;
    
};

class Commander : public PersonBase
{
public:
    Commander(const std::string& name,
                    const mbedtls_x509_crt& publicCert,
                    const mbedtls_pk_context& privateKey,
                    const mbedtls_x509_crt& publicCA,
                    uint16_t soldierNumber, uint16_t intervalMS);

    const std::string& getName() const;
    const mbedtls_x509_crt& getPublicCert() const;
    const mbedtls_x509_crt& getCAPublicCert() const;
    uint16_t getCommanderNumber() const;
    uint16_t getCurrentHeartRate() const;
    const crypto::Key256& getGMK() const {
        return GMK;
    }
    const crypto::Key256& getCompGMK() const {
        return CompGMK;
    }
    const uint16_t getIntervalMS() const
    {
        return intervalMS;
    }

    const std::vector<uint8_t>& getComp();
    

    void setName(const std::string& name);
    void setPublicCert(const std::string& publicCert);
    void setPrivateKey(const std::string& privateKey);
    void setCAPublicCert(const std::string& caPublicCert);
    void setCommanderNumber(uint16_t soldierNumber);
    void setCurrentHeartRate(uint16_t heartRate);
    void setGMK(const crypto::Key256& gmk);
    void setCompGMK(const crypto::Key256& gmk);
    void setCompromised(const uint8_t id);
    void setComp(const std::vector<uint8_t>& comp);
    void setMissing(const std::vector<uint8_t>& missing);
    void setMissing(uint8_t id);

    const mbedtls_pk_context& getPrivateKey() const;

    const std::vector<float>& getFrequencies() const;
    const std::vector<uint8_t>& getMissing() const;
    const std::vector<uint8_t> getNotMissing() const;
    void appendFrequencies(const std::vector<float>& freqs);
    

    bool isComp(uint8_t id);
    void clear();

private:
    std::string name;
    uint16_t commanderNumber;
    uint16_t currentHeartRate;
    uint16_t intervalMS;

    std::vector<float> frequencies;
    std::vector<uint8_t> comp;
    std::vector<uint8_t> missing;

    crypto::Key256 GK;
    crypto::Key256 GMK = []() {
        crypto::Key256 key{};
        const char* raw = "0123456789abcdef0123456789abcdef"; 
        std::memcpy(key.data(), raw, 32);
        return key;
    }();

    crypto::Key256 CompGMK = []() {
        crypto::Key256 key{};
        const char* raw = "11111111111111111111111111111111"; 
        std::memcpy(key.data(), raw, 32);
        return key;
    }();

    void addComp(const uint8_t id);
    bool isMissing(uint8_t id);

    mbedtls_pk_context privateKey;
    mbedtls_x509_crt caCertificate;
    mbedtls_x509_crt ownCertificate;
};