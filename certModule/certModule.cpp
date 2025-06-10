#include "certModule.h"
#include "mbedtls/base64.h"
#include "mbedtls/error.h"
#include "mbedtls/pk.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/version.h"
#include "mbedtls/pem.h"
#include "mbedtls/rsa.h"


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

std::string certModule::certToString(const mbedtls_x509_crt& cert) {
    unsigned char buf[4096];
    size_t olen = 0;

    int ret = mbedtls_pem_write_buffer(
                  "-----BEGIN CERTIFICATE-----\n",
                  "-----END CERTIFICATE-----\n",
                  cert.raw.p,
                  cert.raw.len,
                  buf,
                  sizeof(buf),
                  &olen
              );
    if (ret != 0) {
        return "";
    }

    return std::string(reinterpret_cast<char*>(buf), olen);
}

std::string certModule::privateKeyToString(const mbedtls_pk_context& privateKey) {
    static unsigned char buf[4096];
    char ver[100];

    Serial.println("mbedTLS version: ");
    mbedtls_version_get_string(ver);
    Serial.println(ver);
    mbedtls_pk_type_t t = mbedtls_pk_get_type(&privateKey);
    Serial.print("Key type is: ");
    switch (t) {
    case MBEDTLS_PK_RSA:
        Serial.println("MBEDTLS_PK_RSA");
        break;
    case MBEDTLS_PK_ECKEY:
        Serial.println("MBEDTLS_PK_ECKEY");
        break;
    case MBEDTLS_PK_NONE:
        Serial.println("MBEDTLS_PK_NONE");
        break;
    case MBEDTLS_PK_OPAQUE:
        Serial.println("MBEDTLS_PK_OPAQUE (likely hardware key, not supported by pem_write)");
        break;
    default:
        Serial.printf("Other type: %d\n", (int)t);
        break;
    }
    int ret = mbedtls_pk_write_key_pem(const_cast<mbedtls_pk_context*>(&privateKey), buf, sizeof(buf));
    if (ret != 0) {
        char errbuf[100];
        mbedtls_strerror(ret, errbuf, sizeof(errbuf));
        Serial.printf("❌ Private key PEM conversion failed: %s\n", errbuf);
        return "";
    }
    return std::string(reinterpret_cast<char*>(buf));
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

NameId certModule::parseNameIdFromCertPem(const std::string& pem) {

    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);
    if (mbedtls_x509_crt_parse(&cert, reinterpret_cast<const unsigned char*>(pem.c_str()), pem.size() + 1) != 0)
    {
        mbedtls_x509_crt_free(&cert);
        throw std::runtime_error("Failed to parse certificate PEM");
    }

    char cnBuf[128] = {0};
    for (auto p = &cert.subject; p; p = p->next) {
        if (MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &p->oid) == 0) {
            size_t len = std::min<size_t>(p->val.len, sizeof(cnBuf) - 1);
            std::memcpy(cnBuf, p->val.p, len);
            cnBuf[len] = '\0';
            break;
        }
    }
    
    mbedtls_x509_crt_free(&cert);

    std::string cnStr(cnBuf);
    auto pos = cnStr.find_last_of(' ');
    if (pos == std::string::npos)
        throw std::runtime_error("CN format invalid, expected 'Name Surname ID'");

    NameId result;
    result.name = cnStr.substr(0, pos);
    result.id = static_cast<uint16_t>(std::stoul(cnStr.substr(pos + 1)));
    return result;
}

bool certModule::encryptWithPublicKeyPem(const std::string& pemCert,
                             const std::string& data,
                             std::vector<uint8_t>& output)
{
    int ret;
    mbedtls_x509_crt cert;
    mbedtls_x509_crt_init(&cert);

    ret = mbedtls_x509_crt_parse(
        &cert,
        reinterpret_cast<const unsigned char*>(pemCert.c_str()),
        pemCert.size() + 1
    );
    
    if (ret != 0) {
        Serial.printf("❌ CRT parse failed: -0x%04X\n", -ret);
        mbedtls_x509_crt_free(&cert);
        return false;
    }

    bool ok = encryptWithPublicKey(cert, data, output);

    mbedtls_x509_crt_free(&cert);
    return ok;
}

//handling base64 functions
String certModule::toBase64(const std::vector<uint8_t>& input) {
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


bool certModule::decodeBase64(const String& input,
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
