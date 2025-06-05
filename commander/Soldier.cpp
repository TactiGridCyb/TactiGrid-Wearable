#include "Soldier.h"
#include <cstring>
#include <stdexcept>

Soldier::Soldier(const std::string& name,
                 const mbedtls_x509_crt& publicCert,
                 const mbedtls_pk_context& privateKey,
                 const mbedtls_x509_crt& caPublicCert,
                 uint8_t soldierNumber, uint16_t intervalMS)
    : name(name),
      soldierNumber(soldierNumber),
      currentHeartRate(0),
      intervalMS(intervalMS)
{
    Serial.println("Soldier::Soldier");
    mbedtls_pk_init(&this->privateKey);
    mbedtls_x509_crt_init(&this->caCertificate);
    mbedtls_x509_crt_init(&this->ownCertificate);

    if (mbedtls_x509_crt_parse_der(&this->ownCertificate, publicCert.raw.p, publicCert.raw.len) != 0) {
        throw std::runtime_error("Failed to copy public certificate");
    }

    Serial.println("mbedtls_x509_crt_parse_der");
    if (mbedtls_x509_crt_parse_der(&this->caCertificate, caPublicCert.raw.p, caPublicCert.raw.len) != 0) {
        throw std::runtime_error("Failed to copy CA certificate");
    }

    static unsigned char buf[4096];
    int ret = mbedtls_pk_write_key_der(const_cast<mbedtls_pk_context*>(&privateKey), buf, sizeof(buf));
    if (ret > 0) {
        if (mbedtls_pk_parse_key(&this->privateKey, buf + sizeof(buf) - ret, ret, nullptr, 0) != 0) {
            throw std::runtime_error("Failed to copy private key");
        }
    } else {
        throw std::runtime_error("Failed to export private key");
    }

    Serial.println("mbedtls_pk_write_key_der");

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
    this->others.clear();
    this->insertionOrder.clear();
    this->comp.clear();

    mbedtls_pk_free(&this->privateKey);
    mbedtls_x509_crt_free(&this->ownCertificate);
    mbedtls_x509_crt_free(&this->caCertificate);
    
}