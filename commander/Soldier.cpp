#include "Soldier.h"
#include <cstring>
#include <stdexcept>

Soldier::Soldier(const std::string& name,
                 mbedtls_x509_crt publicCert,
                 mbedtls_pk_context privateKey,
                 mbedtls_x509_crt caPublicCert,
                 uint8_t soldierNumber, uint16_t intervalMS)
    : name(name),
      soldierNumber(soldierNumber),
      currentHeartRate(0),
      intervalMS(intervalMS)
{
    Serial.println("Soldier::Soldier");
    mbedtls_x509_crt_init(&this->caCertificate);
    mbedtls_x509_crt_init(&this->ownCertificate);
    mbedtls_pk_init(&this->privateKey);

    int ret = mbedtls_x509_crt_parse(&this->ownCertificate,
                                     publicCert.raw.p, publicCert.raw.len);
    if (ret != 0) {
        Serial.printf("❌ x509 copy (own) failed: -0x%04X\n", -ret);
    }

    ret = mbedtls_x509_crt_parse(&this->caCertificate,
                                 caPublicCert.raw.p, caPublicCert.raw.len);
    if (ret != 0) {
        Serial.printf("❌ x509 copy (ca) failed: -0x%04X\n", -ret);
    }

    unsigned char pemBuf[4096];
    ret = mbedtls_pk_write_key_pem(&privateKey, pemBuf, sizeof(pemBuf));
    if (ret != 0) 
    {
        Serial.printf("❌ pk_write_key_pem failed: -0x%04X \n", -ret);
    } else {
        ret = mbedtls_pk_parse_key(&this->privateKey,
                                   pemBuf, strlen((char*)pemBuf) + 1,
                                   nullptr, 0);
        if (ret != 0) 
        {
            Serial.printf("❌ pk_parse_key(copy) failed: -0x%04X\n", -ret);
        }
    }

}

const std::string& Soldier::getName() const {
    return name;
}

const mbedtls_x509_crt& Soldier::getPublicCert() const {
    return ownCertificate;
}

const mbedtls_x509_crt& Soldier::getCAPublicCert() const {
    return caCertificate;
}
void Soldier::setGMK(const crypto::Key256& gmk) {
    this->GMK = gmk;
}

const mbedtls_pk_context& Soldier::getPrivateKey() const
{
    return privateKey;
}

uint8_t Soldier::getSoldierNumber() const {
    return soldierNumber;
}

uint16_t Soldier::getCurrentHeartRate() const {
    return currentHeartRate;
}

void Soldier::setName(const std::string& name) {
    this->name = name;
}

void Soldier::setPublicCert(const std::string& publicCert) {
    mbedtls_x509_crt_free(&ownCertificate);
    mbedtls_x509_crt_init(&ownCertificate);
    if (mbedtls_x509_crt_parse(&ownCertificate, reinterpret_cast<const unsigned char*>(publicCert.c_str()), publicCert.size() + 1) != 0) {
        throw std::runtime_error("Failed to parse public certificate");
    }
}

void Soldier::setGK(const crypto::Key256& gk)
{
    this->GK = gk;
}


void Soldier::setPrivateKey(const std::string& privateKey) {
    mbedtls_pk_free(&this->privateKey);
    mbedtls_pk_init(&this->privateKey);
    if (mbedtls_pk_parse_key(&this->privateKey, reinterpret_cast<const unsigned char*>(privateKey.c_str()), privateKey.size() + 1, nullptr, 0) != 0) {
        throw std::runtime_error("Failed to parse private key");
    }
}

void Soldier::setCAPublicCert(const std::string& caPublicCert) {
    mbedtls_x509_crt_free(&caCertificate);
    mbedtls_x509_crt_init(&caCertificate);
    if (mbedtls_x509_crt_parse(&caCertificate, reinterpret_cast<const unsigned char*>(caPublicCert.c_str()), caPublicCert.size() + 1) != 0) {
        throw std::runtime_error("Failed to parse CA public certificate");
    }
}

void Soldier::setSoldierNumber(uint8_t soldierNumber) {
    this->soldierNumber = soldierNumber;
}

void Soldier::setCurrentHeartRate(uint16_t heartRate) {
    this->currentHeartRate = heartRate;
}

const std::vector<float>& Soldier::getFrequencies() const {
    return frequencies;
}

void Soldier::appendFrequencies(const std::vector<float>& freqs) {
    frequencies.insert(frequencies.end(), freqs.begin(), freqs.end());
}


void Soldier::clear()
{
    this->commanders.clear();
    this->commandersInsertionOrder.clear();
    this->soldiers.clear();

    mbedtls_pk_free(&this->privateKey);
    mbedtls_x509_crt_free(&this->ownCertificate);
    mbedtls_x509_crt_free(&this->caCertificate);
    
}