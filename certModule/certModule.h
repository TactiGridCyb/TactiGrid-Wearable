#pragma once
#include "../commander-config/commander-config.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/oid.h"
#include "mbedtls/entropy.h"
#include <cstring>

struct NameId {
    std::string name;
    uint16_t id;
};

class certModule {
public:
    certModule();
    ~certModule();

    bool loadFromConfig(const CommanderConfigModule& config);

    bool encryptWithPrivateKey(const uint8_t* input, size_t inputLen,
                               uint8_t* output, size_t& outputLen);

    bool verifyCertificate();
    bool loadSingleCertificate(const String& pemCert);

    static bool verifyCertificate(mbedtls_x509_crt* certToVerify, mbedtls_x509_crt* caCert);
   
    static bool encryptWithPublicKey(const mbedtls_x509_crt& cert, const std::string& data,
                                    std::vector<uint8_t>& output);
   
    static bool decryptWithPrivateKey(mbedtls_pk_context& privateKey,
                                      const std::vector<uint8_t>& input,
                                      std::string& output);
                                    
    static certModule fromCertificateString(const std::string& certStr);

    static certModule fromPrivateKeyString(const std::string& privateKeyStr);
    
    static NameId parseNameIdFromCertPem(const std::string& pem);

private:
    mbedtls_pk_context privateKey;
    mbedtls_x509_crt certificate;
    mbedtls_x509_crt caCertificate;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
};
