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

//load from commander config
bool CryptoHelper::loadFromCommanderConfig(const CommanderConfigModule& cfg) {
    // 0) Initialize all contexts
    mbedtls_pk_init(&privateKey);
    mbedtls_x509_crt_init(&certificate);
    mbedtls_x509_crt_init(&caCertificate);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    // 1) Seed the DRBG
    int ret = mbedtls_ctr_drbg_seed(
        &ctr_drbg,
        mbedtls_entropy_func,
        &entropy,
        nullptr, 0
    );
    if (ret != 0) {
        char err[100];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ CTR_DRBG seed failed: -0x%04X (%s)\n", -ret, err);
        return false;
    }

    // 2) A little helper to decode Base64 into a std::string
    auto decodeB64 = [&](const String& b64)-> std::string {
        size_t inLen  = b64.length();
        size_t outLen = (inLen * 3) / 4 + 4;
        std::vector<uint8_t> buf(outLen);
        size_t actual = 0;

        int r = mbedtls_base64_decode(
            buf.data(), buf.size(), &actual,
            (const uint8_t*)b64.c_str(), inLen
        );
        if (r != 0) {
            char e[100];
            mbedtls_strerror(r, e, sizeof(e));
            Serial.printf("❌ Base64 decode failed: -0x%04X (%s)\n", -r, e);
            return "";
        }

        // null-terminate so mbedTLS can parse it as PEM
        buf.resize(actual);
        buf.push_back('\0');
        return std::string((char*)buf.data());
    };

    // 3) Decode & parse the private key
    {
        std::string keyPem = decodeB64(cfg.getPrivateKeyPEM());
        if (keyPem.empty()) return false;

        ret = mbedtls_pk_parse_key(
            &privateKey,
            (const uint8_t*)keyPem.c_str(),
            keyPem.size() + 1,
            nullptr, 0
        );
        if (ret != 0) {
            char err[100];
            mbedtls_strerror(ret, err, sizeof(err));
            Serial.printf("❌ Private key parse failed: -0x%04X (%s)\n", -ret, err);
            return false;
        }
    }

    // 4) Decode & parse the commander certificate
    {
        std::string certPem = decodeB64(cfg.getCertificatePEM());
        if (certPem.empty()) return false;

        ret = mbedtls_x509_crt_parse(
            &certificate,
            (const uint8_t*)certPem.c_str(),
            certPem.size() + 1
        );
        if (ret != 0) {
            char err[100];
            mbedtls_strerror(ret, err, sizeof(err));
            Serial.printf("❌ Certificate parse failed: -0x%04X (%s)\n", -ret, err);
            return false;
        }
    }

    // 5) Decode & parse the CA certificate
    {
        std::string caPem = decodeB64(cfg.getCaCertificatePEM());
        if (caPem.empty()) return false;

        ret = mbedtls_x509_crt_parse(
            &caCertificate,
            (const uint8_t*)caPem.c_str(),
            caPem.size() + 1
        );
        if (ret != 0) {
            char err[100];
            mbedtls_strerror(ret, err, sizeof(err));
            Serial.printf("❌ CA cert parse failed: -0x%04X (%s)\n", -ret, err);
            return false;
        }
    }

    Serial.println("✅ Successfully loaded private key, certificate, and CA cert");
    return true;
}



//load from soldiers config
bool CryptoHelper::loadFromSoldierConfig(const SoldierConfigModule& cfg) {
    // 0) Initialize all contexts
    mbedtls_pk_init(&privateKey);
    mbedtls_x509_crt_init(&certificate);
    mbedtls_x509_crt_init(&caCertificate);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    // 1) Seed the DRBG
    int ret = mbedtls_ctr_drbg_seed(
        &ctr_drbg,
        mbedtls_entropy_func,
        &entropy,
        nullptr, 0
    );
    if (ret != 0) {
        char err[100];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ CTR_DRBG seed failed: -0x%04X (%s)\n", -ret, err);
        return false;
    }

    // 2) A little helper to decode Base64 into a std::string
    auto decodeB64 = [&](const String& b64)-> std::string {
        size_t inLen  = b64.length();
        size_t outLen = (inLen * 3) / 4 + 4;
        std::vector<uint8_t> buf(outLen);
        size_t actual = 0;

        int r = mbedtls_base64_decode(
            buf.data(), buf.size(), &actual,
            (const uint8_t*)b64.c_str(), inLen
        );
        if (r != 0) {
            char e[100];
            mbedtls_strerror(r, e, sizeof(e));
            Serial.printf("❌ Base64 decode failed: -0x%04X (%s)\n", -r, e);
            return "";
        }

        // null-terminate so mbedTLS can parse it as PEM
        buf.resize(actual);
        buf.push_back('\0');
        return std::string((char*)buf.data());
    };

    // 3) Decode & parse the private key
    {
        std::string keyPem = decodeB64(cfg.getPrivateKeyPEM());
        if (keyPem.empty()) return false;

        ret = mbedtls_pk_parse_key(
            &privateKey,
            (const uint8_t*)keyPem.c_str(),
            keyPem.size() + 1,
            nullptr, 0
        );
        if (ret != 0) {
            char err[100];
            mbedtls_strerror(ret, err, sizeof(err));
            Serial.printf("❌ Private key parse failed: -0x%04X (%s)\n", -ret, err);
            return false;
        }
    }

    // 4) Decode & parse the commander certificate
    {
        std::string certPem = decodeB64(cfg.getCertificatePEM());
        if (certPem.empty()) return false;

        ret = mbedtls_x509_crt_parse(
            &certificate,
            (const uint8_t*)certPem.c_str(),
            certPem.size() + 1
        );
        if (ret != 0) {
            char err[100];
            mbedtls_strerror(ret, err, sizeof(err));
            Serial.printf("❌ Certificate parse failed: -0x%04X (%s)\n", -ret, err);
            return false;
        }
    }

    // 5) Decode & parse the CA certificate
    {
        std::string caPem = decodeB64(cfg.getCaCertificatePEM());
        if (caPem.empty()) return false;

        ret = mbedtls_x509_crt_parse(
            &caCertificate,
            (const uint8_t*)caPem.c_str(),
            caPem.size() + 1
        );
        if (ret != 0) {
            char err[100];
            mbedtls_strerror(ret, err, sizeof(err));
            Serial.printf("❌ CA cert parse failed: -0x%04X (%s)\n", -ret, err);
            return false;
        }
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
