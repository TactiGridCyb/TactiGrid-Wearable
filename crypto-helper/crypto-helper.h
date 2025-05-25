// crypto-helper.h
#pragma once
#include "../commander-config/commander-config.h"
#include "../soldier-config/soldier-config.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

class CryptoHelper {
public:
    CryptoHelper();
    ~CryptoHelper();

    bool loadFromCommanderConfig(const CommanderConfigModule& config);
    bool loadFromSoldierConfig(const SoldierConfigModule& config);

    // Encrypts input with the private key (usually you'd sign instead)
    bool encryptWithPrivateKey(const uint8_t* input, size_t inputLen,
                               uint8_t* output, size_t& outputLen);

    // Verifies a certificate against the CA
    bool verifyCertificate();
    bool loadSingleCertificate(const String& pemCert);


private:
    mbedtls_pk_context privateKey;
    mbedtls_x509_crt certificate;
    mbedtls_x509_crt caCertificate;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
};
