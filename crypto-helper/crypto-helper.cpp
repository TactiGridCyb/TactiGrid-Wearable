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

    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg,
                                    mbedtls_entropy_func,
                                    &entropy,
                                    nullptr, 0);
    if (ret != 0) {
        char errbuf[128];
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        Serial.printf("❌ CTR_DRBG seed failed in ctor: %s\n", errbuf);
    }
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



bool CryptoHelper::loadFromSoldierConfig(const SoldierConfigModule& cfg) {
    // 0) Initialize all contexts
    mbedtls_pk_init(&privateKey);
    mbedtls_x509_crt_init(&certificate);
    mbedtls_x509_crt_init(&caCertificate);
    mbedtls_ctr_drbg_init(&ctr_drbg);
    mbedtls_entropy_init(&entropy);

    // 1) Seed the DRBG
    int ret = mbedtls_ctr_drbg_seed(&ctr_drbg,
                                    mbedtls_entropy_func,
                                    &entropy,
                                    nullptr, 0);
    if (ret != 0) {
        char err[100];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ CTR_DRBG seed failed: -0x%04X (%s)\n", -ret, err);
        return false;
    }

    // 2) Base64→PEM helper
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
        buf.resize(actual);
        buf.push_back('\0');  // allow PEM parser to see NUL
        return std::string((char*)buf.data());
    };

    // 3) Parse private key
    {
        std::string keyPem = decodeB64(cfg.getPrivateKeyPEM());
        if (keyPem.empty()) return false;
        ret = mbedtls_pk_parse_key(&privateKey,
                                   (const uint8_t*)keyPem.c_str(),
                                   keyPem.size() + 1,
                                   nullptr, 0);
        if (ret != 0) {
            char err[100];
            mbedtls_strerror(ret, err, sizeof(err));
            Serial.printf("❌ Private key parse failed: -0x%04X (%s)\n", -ret, err);
            return false;
        }
    }

    // 4) Parse soldier’s certificate
    {
        std::string certPem = decodeB64(cfg.getCertificatePEM());
        if (certPem.empty()) return false;
        ret = mbedtls_x509_crt_parse(&certificate,
                                     (const uint8_t*)certPem.c_str(),
                                     certPem.size() + 1);
        if (ret != 0) {
            char err[100];
            mbedtls_strerror(ret, err, sizeof(err));
            Serial.printf("❌ Certificate parse failed: -0x%04X (%s)\n", -ret, err);
            return false;
        }
        Serial.println("✅ Soldier certificate parsed — now extracting pubkey…");
    }

    // 4.5) Extract & print the public key as Base64 DER
    {
        // write DER pubkey into a fixed-size buffer
        unsigned char der_buf[800];
        int der_len = mbedtls_pk_write_pubkey_der(&certificate.pk,
                                                  der_buf, sizeof(der_buf));
        if (der_len < 0) {
            char err[100];
            mbedtls_strerror(der_len, err, sizeof(err));
            Serial.printf("❌ DER write failed: -0x%04X (%s)\n", -der_len, err);
        } else {
            // der_buf is right-aligned: start at this pointer
            unsigned char* p = der_buf + sizeof(der_buf) - der_len;

            // compute Base64 length & encode
            size_t b64_max = 4 * ((der_len + 2) / 3) + 1;
            std::vector<uint8_t> b64(b64_max);
            size_t olen = 0;
            int berr = mbedtls_base64_encode(
                b64.data(), b64.size(), &olen,
                p, der_len
            );
            if (berr == 0) {
                b64[olen] = '\0';
                Serial.println("📢 Soldier public key (Base64 DER):");
                Serial.println((char*)b64.data());
            } else {
                char e[100];
                mbedtls_strerror(berr, e, sizeof(e));
                Serial.printf("❌ DER→Base64 encode failed: -0x%04X (%s)\n", -berr, e);
            }
        }
    }

    // 5) Parse CA certificate
    {
        std::string caPem = decodeB64(cfg.getCaCertificatePEM());
        if (caPem.empty()) return false;
        ret = mbedtls_x509_crt_parse(&caCertificate,
                                     (const uint8_t*)caPem.c_str(),
                                     caPem.size() + 1);
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

//handling base64 functions
String CryptoHelper::toBase64(const std::vector<uint8_t>& input) {
    size_t outLen = 4 * ((input.size() + 2) / 3);
    std::vector<uint8_t> outBuf(outLen + 1);
    size_t actualLen = 0;
    int ret = mbedtls_base64_encode(outBuf.data(), outBuf.size(), &actualLen, input.data(), input.size());
    if (ret != 0) {
        Serial.println("❌ Base64 encode failed");
        return "";
    }
    outBuf[actualLen] = '\0';
    return String((char*)outBuf.data());
}

// bool CryptoHelper::decodeBase64(const String& input,
//                                         std::vector<uint8_t>& output) {
//     Serial.println("enter: decode function");
//   size_t inputLen = input.length();
//   size_t decodedLen = (inputLen * 3) / 4 + 2;
//   output.resize(decodedLen);
//   size_t outLen = 0;
//   int ret = mbedtls_base64_decode(output.data(), output.size(), &outLen,
//                                 (const uint8_t*)input.c_str(), inputLen);
//     if (ret != 0) {
//         char errBuf[128];
//         mbedtls_strerror(ret, errBuf, sizeof(errBuf));
//         Serial.printf("❌ Base64 decode failed: -0x%04X (%s)\n", -ret, errBuf);
//         return false;
//     }
//     // *shrink to actual size!*
//     output.resize(outLen);
//     return true;
// }

bool CryptoHelper::decodeBase64(const String& input,
                                std::vector<uint8_t>& output) {
    // 1) strip invalid characters
    std::string b64;
    b64.reserve(input.length());
    for (char c : input) {
        if ((c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
             c == '+' || c == '/' || c == '=') {
            b64.push_back(c);
        }
    }

    // ─── ADD THIS SERIAL PRINT ───────────────────────────────────────────────────
    Serial.println("🔍 [DEBUG] cleaned Base64 before decode:");
    Serial.println(b64.c_str());
    // ─────────────────────────────────────────────────────────────────────────────


    // 2) now decode
    size_t inLen = b64.size();
    size_t maxOut = (inLen * 3) / 4 + 3;
    output.resize(maxOut);
    size_t actual = 0;

    int ret = mbedtls_base64_decode(
        output.data(), output.size(), &actual,
        (const uint8_t*)b64.data(), inLen
    );
    if (ret != 0) {
        char err[128];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ Base64 decode failed: -0x%04X (%s)\n", -ret, err);
        return false;
    }
    output.resize(actual);
    return true;
}


CryptoHelper CryptoHelper::fromPrivateKeyString(const std::string& privateKeyStr) {
    CryptoHelper certObj;
    int ret = mbedtls_pk_parse_key(&certObj.privateKey, (const unsigned char*)privateKeyStr.c_str(), privateKeyStr.size() + 1, NULL, 0);
    if (ret != 0) {
        char err[256];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ Failed to parse private key string: %s\n", err);
    }
    return certObj;
}

CryptoHelper CryptoHelper::fromCertificateString(const std::string& certStr) {
    CryptoHelper certObj;
    int ret = mbedtls_x509_crt_parse(&certObj.certificate, (const unsigned char*)certStr.c_str(), certStr.size() + 1);
    if (ret != 0) {
        char err[256];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ Failed to parse certificate string: %s\n", err);
    }
    return certObj;
}

bool CryptoHelper::decryptWithPrivateKey(const mbedtls_pk_context& privateKey,
                                       const std::vector<uint8_t>& input,
                                       std::string& output)
{
    Serial.printf("🔐 decryptWithPrivateKey() called\n");
    Serial.printf("  • Encrypted input length: %u bytes\n", (unsigned)input.size());

    // 1) Figure out buffer size
    size_t keyLen = mbedtls_pk_get_len(const_cast<mbedtls_pk_context*>(&privateKey));
    Serial.printf("  • RSA key length: %u bytes\n", (unsigned)keyLen);

    std::vector<unsigned char> buffer(keyLen);
    size_t outputLen = 0;

    // 2) Perform RSA‐OAEP decryption
    int ret = mbedtls_pk_decrypt(
        const_cast<mbedtls_pk_context*>(&privateKey),
        input.data(), input.size(),
        buffer.data(), &outputLen, buffer.size(),
        nullptr, nullptr
    );

    if (ret != 0) {
        char err[128];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ mbedtls_pk_decrypt failed: -0x%04X (%s)\n", -ret, err);
        return false;
    }

    Serial.printf("  ✅ mbedtls_pk_decrypt succeeded, outputLen=%u\n", (unsigned)outputLen);

    // 3) Copy to std::string
    output.assign((char*)buffer.data(), outputLen);
    Serial.printf("  ✅ Returning plaintext of %u bytes\n", (unsigned)output.size());

    return true;
}





bool CryptoHelper::encryptWithPublicKey(const mbedtls_x509_crt& cert, const std::string& data,
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

bool CryptoHelper::loadPublicKeyFromBase64Der(const String& b64Der,
                                              mbedtls_pk_context& keyCtx)
{
    Serial.println("🔍 [DEBUG] loadPublicKeyFromBase64Der got this input:");
    Serial.println(b64Der.substring(0, 64));   // first 64 chars

    mbedtls_pk_init(&keyCtx);

    // 1) Decode Base64 → DER bytes
    std::vector<uint8_t> der;
    if (!decodeBase64(b64Der, der)) {
        Serial.println("❌ loadPublicKey: Base64 decode failed");
        return false;
    }

    // <<< ADD THIS DEBUG BLOCK >>>
    Serial.println("🔍 [DEBUG] DER blob decoded:");
    Serial.printf("    byte length = %u\n", (unsigned)der.size());
    Serial.print("    first bytes = ");
    for (size_t i = 0; i < der.size() && i < 16; ++i) {
      Serial.printf("%02X ", der[i]);
    }
    Serial.println("…");
    // <<< end debug >>>

    // 2) Parse DER
    int ret = mbedtls_pk_parse_public_key(
        &keyCtx,
        der.data(), der.size()
    );
    if (ret != 0) {
        char err[128];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ parse_public_key failed: -0x%04X (%s)\n", -ret, err);
        mbedtls_pk_free(&keyCtx);
        return false;
    }

    return true;
}


bool CryptoHelper::encryptBase64Payload(const String& b64Plain,
                                        mbedtls_pk_context* peerPubKey,
                                        String& b64Cipher)
{
    // 1) Base64→raw
    std::vector<uint8_t> plain;
    if (!decodeBase64(b64Plain, plain)) return false;

    // 2) Encrypt with RSA-OAEP
    size_t keyLen = mbedtls_pk_get_len(peerPubKey);
    std::vector<uint8_t> cipher(keyLen + 16);
    size_t olen = 0;

    int ret = mbedtls_pk_encrypt(
        peerPubKey,
        plain.data(), plain.size(),
        cipher.data(), &olen, cipher.size(),
        mbedtls_ctr_drbg_random, &ctr_drbg
    );
    if (ret != 0) {
        char err[128];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ encryptBase64Payload failed: -0x%04X (%s)\n", -ret, err);
        return false;
    }

    cipher.resize(olen);
    b64Cipher = toBase64(cipher);
    return true;
}