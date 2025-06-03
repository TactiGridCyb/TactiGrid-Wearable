#include "Commander.h"
#include <stdexcept>
#include <cstring>

Commander::Commander(const std::string& name,
                               const mbedtls_x509_crt& publicCert,
                               const mbedtls_pk_context& privateKey,
                               const mbedtls_x509_crt& publicCA,
                               uint16_t soldierNumber, uint16_t intervalMS)
    : name(name),
      commanderNumber(soldierNumber),
      currentHeartRate(0),
      intervalMS(intervalMS)
{
    mbedtls_pk_init(&this->privateKey);
    mbedtls_x509_crt_init(&this->caCertificate);
    mbedtls_x509_crt_init(&this->ownCertificate);

    if (mbedtls_x509_crt_parse_der(&this->ownCertificate, publicCert.raw.p, publicCert.raw.len) != 0) {
        throw std::runtime_error("Failed to copy public certificate");
    }

    if (mbedtls_x509_crt_parse_der(&this->caCertificate, publicCA.raw.p, publicCA.raw.len) != 0) {
        throw std::runtime_error("Failed to copy CA certificate");
    }

    unsigned char buf[4096];
    int ret = mbedtls_pk_write_key_der(const_cast<mbedtls_pk_context*>(&privateKey), buf, sizeof(buf));
    if (ret > 0) {
        if (mbedtls_pk_parse_key(&this->privateKey, buf + sizeof(buf) - ret, ret, nullptr, 0) != 0) {
            throw std::runtime_error("Failed to copy private key");
        }
    } else {
        throw std::runtime_error("Failed to export private key");
    }
}

const std::string& Commander::getName() const {
    return name;
}

const mbedtls_x509_crt& Commander::getPublicCert() const {
    return ownCertificate;
}

const mbedtls_x509_crt& Commander::getCAPublicCert() const {
    return caCertificate;
}

void Commander::setGMK(const crypto::Key256& gmk) {
    this->GMK = gmk;
}

void Commander::setCompGMK(const crypto::Key256& gmk) {
    this->CompGMK = gmk;
}

uint16_t Commander::getCommanderNumber() const {
    return commanderNumber;
}

uint16_t Commander::getCurrentHeartRate() const {
    return currentHeartRate;
}

void Commander::setName(const std::string& name) {
    this->name = name;
}

void Commander::setPublicCert(const std::string& publicCert) {
    mbedtls_x509_crt_free(&ownCertificate);
    mbedtls_x509_crt_init(&ownCertificate);
    if (mbedtls_x509_crt_parse(&ownCertificate, reinterpret_cast<const unsigned char*>(publicCert.c_str()), publicCert.size() + 1) != 0) {
        throw std::runtime_error("Failed to parse public certificate");
    }
}

void Commander::setPrivateKey(const std::string& privateKey) {
    mbedtls_pk_free(&this->privateKey);
    mbedtls_pk_init(&this->privateKey);
    if (mbedtls_pk_parse_key(&this->privateKey, reinterpret_cast<const unsigned char*>(privateKey.c_str()), privateKey.size() + 1, nullptr, 0) != 0) {
        throw std::runtime_error("Failed to parse private key");
    }
}

void Commander::setCAPublicCert(const std::string& caPublicCert) {
    mbedtls_x509_crt_free(&caCertificate);
    mbedtls_x509_crt_init(&caCertificate);
    if (mbedtls_x509_crt_parse(&caCertificate, reinterpret_cast<const unsigned char*>(caPublicCert.c_str()), caPublicCert.size() + 1) != 0) {
        throw std::runtime_error("Failed to parse CA public certificate");
    }
}

void Commander::setCommanderNumber(uint16_t commanderNumber) {
    this->commanderNumber = commanderNumber;
}

void Commander::setCurrentHeartRate(uint16_t heartRate) {
    this->currentHeartRate = heartRate;
}

const mbedtls_pk_context& Commander::getPrivateKey() const
{
    return privateKey;
}

const std::vector<float>& Commander::getFrequencies() const {
    return frequencies;
}

void Commander::appendFrequencies(const std::vector<float>& freqs) {
    frequencies.insert(frequencies.end(), freqs.begin(), freqs.end());
}

void Commander::clear()
{
    this->others.clear();
    this->insertionOrder.clear();
    this->comp.clear();

    mbedtls_pk_free(&this->privateKey);
    mbedtls_x509_crt_free(&this->ownCertificate);
    mbedtls_x509_crt_free(&this->caCertificate);
    
}