#include "CommanderModule.h"
#include <stdexcept>
#include <cstring>

CommanderModule::CommanderModule(const std::string& name,
                               const mbedtls_x509_crt& publicCert,
                               const mbedtls_pk_context& privateKey,
                               const mbedtls_x509_crt& publicCA,
                               uint16_t soldierNumber, uint16_t intervalMS)
    : name(name),
      soldierNumber(soldierNumber),
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

int CommanderModule::getCurrentHeartRate() const {
    return currentHeartRate;
}

void CommanderModule::setCurrentHeartRate(int heartRate) {
    currentHeartRate = heartRate;
}