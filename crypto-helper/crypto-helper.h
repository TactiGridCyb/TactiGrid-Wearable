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

    static bool verifyCertificate(mbedtls_x509_crt* certToVerify, mbedtls_x509_crt* caCert);
   
    static bool encryptWithPublicKey(const mbedtls_x509_crt& cert, const std::string& data,
                                    std::vector<uint8_t>& output);
   
        // un‐RSA‐OAEP wrap of the AES key
    static bool decryptWithPrivateKey(const mbedtls_pk_context& privateKey,
                                  const std::vector<uint8_t>& input,
                                  std::string& output);

                                    
    static CryptoHelper fromCertificateString(const std::string& certStr);

    static CryptoHelper fromPrivateKeyString(const std::string& privateKeyStr);


    //handling base64 functions
    static bool decodeBase64(const String& input, std::vector<uint8_t>& output);
    static String toBase64(const std::vector<uint8_t>& input);

    /// Load a Base64-DER-encoded public key into a PK context ---- new addition
    static bool loadPublicKeyFromBase64Der(const String& b64Der,
                                           mbedtls_pk_context& keyCtx);

    /// Encrypt a Base64 payload (i.e. you feed it Base64-encoded bytes),
    /// RSA-OAEP it with peerPubKey, and return the result Base64-encoded.
    bool encryptBase64Payload(const String& b64Plain,
                              mbedtls_pk_context* peerPubKey,
                              String& b64Cipher);

    inline mbedtls_ctr_drbg_context& getDrbg() { return ctr_drbg; }
    inline mbedtls_pk_context* getPrivateKey() { return &privateKey; }
    inline mbedtls_x509_crt* getCertificateCtx() { return &certificate; }
    


private:
    mbedtls_pk_context privateKey;
    mbedtls_x509_crt certificate;
    mbedtls_x509_crt caCertificate;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
};
