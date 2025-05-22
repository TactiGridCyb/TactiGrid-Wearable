#include "certModule.h"
#include "mbedtls/base64.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"

certModule::certModule() {
    mbedtls_pk_init(&privateKey);
    mbedtls_x509_crt_init(&certificate);
    mbedtls_x509_crt_init(&caCertificate);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0);
}

certModule::~certModule() {
    mbedtls_pk_free(&privateKey);
    mbedtls_x509_crt_free(&certificate);
    mbedtls_x509_crt_free(&caCertificate);
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
}
bool certModule::loadFromConfig(const CommanderConfigModule& config) {
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


bool certModule::verifyCertificate() {
    uint32_t flags;
    int ret = mbedtls_x509_crt_verify(&certificate, &caCertificate, NULL, NULL, &flags, NULL, NULL);
    if (ret != 0) {
        Serial.printf("❌ Certificate verification failed: -0x%04X, flags: %lu\n", -ret, flags);
        return false;
    }
    Serial.println("✅ Certificate verified successfully.");
    return true;
}

bool certModule::verifyCertificate(mbedtls_x509_crt* certToVerify, mbedtls_x509_crt* caCert)
{
    uint32_t flags;
    int ret = mbedtls_x509_crt_verify(certToVerify, caCert, NULL, NULL, &flags, NULL, NULL);
    if (ret != 0) {
        Serial.printf("❌ Certificate verification failed: -0x%04X, flags: %lu\n", -ret, flags);
        return false;
    }
    Serial.println("✅ Certificate verified successfully.");
    return true;
}


bool certModule::encryptWithPrivateKey(const uint8_t* input, size_t inputLen,
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

bool certModule::encryptWithPublicKey(const mbedtls_x509_crt& cert, const std::string& data,
                                      std::vector<uint8_t>& output) {
    int ret;
    size_t outputLen = 0;
    const mbedtls_pk_context* constPk = &cert.pk;
    mbedtls_pk_context* pk = const_cast<mbedtls_pk_context*>(constPk);
    
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_init(&entropy);
    mbedtls_ctr_drbg_init(&ctr_drbg);

    const char* pers = "certModule_encrypt";
    ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                (const unsigned char*)pers, strlen(pers));
    if (ret != 0) {
        Serial.printf("❌ DRBG seed failed: -0x%04X\n", -ret);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        return false;
    }

    size_t keyLen = mbedtls_pk_get_len(pk);
    output.resize(keyLen);

    ret = mbedtls_pk_encrypt(pk,
                             (const unsigned char*)data.data(), data.size(),
                             output.data(), &outputLen, output.size(),
                             mbedtls_ctr_drbg_random, &ctr_drbg);
    if (ret != 0) {
        Serial.printf("❌ Public key encryption failed: -0x%04X\n", -ret);
        mbedtls_entropy_free(&entropy);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        return false;
    }
    

    output.resize(outputLen);

    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free(&ctr_drbg);

    return true;
}

bool certModule::decryptWithPrivateKey(const mbedtls_pk_context& privateKey,
                                       const std::vector<uint8_t>& input,
                                       std::string& output)
{
    int ret;
    size_t outputLen = 0;

    size_t keyLen = mbedtls_pk_get_len(const_cast<mbedtls_pk_context*>(&privateKey));
    std::vector<unsigned char> buffer(keyLen);

    ret = mbedtls_pk_decrypt(const_cast<mbedtls_pk_context*>(&privateKey),
                             input.data(), input.size(),
                             buffer.data(), &outputLen, buffer.size(),
                             nullptr, nullptr);
    if (ret != 0) {
        Serial.printf("❌ Private key decryption failed: -0x%04X\n", -ret);
        return false;
    }

    output.assign((char*)buffer.data(), outputLen);
    return true;
}

certModule certModule::fromCertificateString(const std::string& certStr) {
    certModule certObj;
    int ret = mbedtls_x509_crt_parse(&certObj.certificate, (const unsigned char*)certStr.c_str(), certStr.size() + 1);
    if (ret != 0) {
        char err[256];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ Failed to parse certificate string: %s\n", err);
    }
    return certObj;
}

certModule certModule::fromPrivateKeyString(const std::string& privateKeyStr) {
    certModule certObj;
    int ret = mbedtls_pk_parse_key(&certObj.privateKey, (const unsigned char*)privateKeyStr.c_str(), privateKeyStr.size() + 1, NULL, 0);
    if (ret != 0) {
        char err[256];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ Failed to parse private key string: %s\n", err);
    }
    return certObj;
}

bool certModule::loadSingleCertificate(const String& pemCert) {
    int ret = mbedtls_x509_crt_parse(&certificate, (const unsigned char*)pemCert.c_str(), pemCert.length() + 1);
    if (ret != 0) {
        char err[256];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ Failed to parse cert chunk: %s\n", err);
        return false;
    }
    return true;
}
