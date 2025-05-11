#include "crypto-helper.h"
#include "mbedtls/base64.h"
#include "mbedtls/error.h"        // ✅ For mbedtls_strerror()
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

CryptoHelper::CryptoHelper() {
    mbedtls_pk_init(&privateKey);
    mbedtls_x509_crt_init(&certificate);
    mbedtls_x509_crt_init(&caCertificate);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
}

CryptoHelper::~CryptoHelper() {
    mbedtls_pk_free(&privateKey);
    mbedtls_x509_crt_free(&certificate);
    mbedtls_x509_crt_free(&caCertificate);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}
bool CryptoHelper::loadFromConfig(const CommanderConfigModule& config) {
    mbedtls_pk_init(&privateKey);
    mbedtls_x509_crt_init(&certificate);
    mbedtls_x509_crt_init(&caCertificate);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, nullptr, 0);
    if (ret != 0) {
        Serial.printf("❌ CTR_DRBG seed failed: -0x%04X\n", -ret);
        return false;
    }

    // Decode base64 PEM function
    auto decodeBase64 = [](const String& input) -> std::string {
        size_t inputLen = input.length();
        size_t outputLen = 0;
        std::vector<uint8_t> buffer((inputLen * 3) / 4 + 2); // +2 for null terminator safety

        int ret = mbedtls_base64_decode(buffer.data(), buffer.size(), &outputLen,
                                        reinterpret_cast<const uint8_t*>(input.c_str()), inputLen);

        if (ret != 0) {
            char errbuf[100];
            mbedtls_strerror(ret, errbuf, sizeof(errbuf));
            Serial.printf("❌ Base64 decode failed: %s\n", errbuf);
            return "";
        }

        buffer[outputLen] = '\0'; // null-terminate for PEM parsing
        return std::string(reinterpret_cast<char*>(buffer.data()));
    };

    // Parse private key
    std::string keyPem = decodeBase64(config.getPrivateKeyPEM());
    if (keyPem.empty() || mbedtls_pk_parse_key(&privateKey,
        reinterpret_cast<const uint8_t*>(keyPem.c_str()), keyPem.length() + 1, nullptr, 0) != 0) {
        Serial.println("❌ Failed to parse private key");
        return false;
    }

    // Parse commander cert
    std::string certPem = decodeBase64(config.getCertificatePEM());
    if (certPem.empty() || mbedtls_x509_crt_parse(&certificate,
        reinterpret_cast<const uint8_t*>(certPem.c_str()), certPem.length() + 1) != 0) {
        Serial.println("❌ Failed to parse certificate");
        return false;
    }

    // Parse CA cert
    std::string caPem = decodeBase64(config.getCaCertificatePEM());  // << match your actual method name
    if (caPem.empty() || mbedtls_x509_crt_parse(&caCertificate,
        reinterpret_cast<const uint8_t*>(caPem.c_str()), caPem.length() + 1) != 0) {
        Serial.println("❌ Failed to parse CA certificate");
        return false;
    }

    Serial.println("✅ Successfully loaded private key, certificate, and CA cert");
    return true;
}


bool CryptoHelper::verifyCertificate() {
    uint32_t flags;
    int ret = mbedtls_x509_crt_verify(&certificate, &caCertificate, NULL, NULL, &flags, NULL, NULL);
    if (ret != 0) {
        Serial.printf("❌ Certificate verification failed: -0x%04X, flags: %lu\n", -ret, flags);
        return false;
    }
    Serial.println("✅ Certificate verified successfully.");
    return true;
}

bool CryptoHelper::encryptWithPrivateKey(const uint8_t* input, size_t inputLen,
                                         uint8_t* output, size_t& outputLen) {
    int ret = mbedtls_pk_encrypt(&privateKey,
                                 input, inputLen,
                                 output, &outputLen, 512,
                                 mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        Serial.printf("❌ Encryption failed: -0x%04X\n", -ret);
        return false;
    }
    return true;
}

bool CryptoHelper::loadSingleCertificate(const String& pemCert) {
    int ret = mbedtls_x509_crt_parse(&certificate, (const unsigned char*)pemCert.c_str(), pemCert.length() + 1);
    if (ret != 0) {
        char err[256];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ Failed to parse cert chunk: %s\n", err);
        return false;
    }
    return true;
}
