// SoldierECDHHandler.cpp
#include "SoldierECDHHandler.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"
#include <mbedtls/error.h>
#include "crypto-helper.h"
#include "mbedtls/ctr_drbg.h"
 #include "mbedtls/entropy.h"  
#include <mbedtls/cipher.h>

SoldierECDHHandler* SoldierECDHHandler::instance = nullptr;

SoldierECDHHandler::SoldierECDHHandler(float freq, SoldierConfigModule* cfg, CryptoHelper& crypt)
    : lora(freq), config(cfg), crypto(crypt), hasResponded(false) {
    instance = this;
}

// void SoldierECDHHandler::begin() {
//     lora.setup(false); // receiver mode
//     lora.setOnReadData([](const uint8_t* pkt, size_t len) {
//         instance->lora.onLoraFileDataReceived(pkt, len);
//     });
//     lora.onFileReceived = handleLoRaData;
// }

void SoldierECDHHandler::begin() {
    instance = this;
    lora.setup(false);
    lora.setOnReadData([](const uint8_t* pkt, size_t len) {
        instance->lora.onLoraFileDataReceived(pkt, len);
    });
    lora.onFileReceived = SoldierECDHHandler::handleLoRaDataStatic;
}


void SoldierECDHHandler::startListening() {
    hasResponded = false;
    lora.setupListening();
}

// ────────────────────────────────────────────────────────────────────────────
// static wrapper: bridges the C‐style callback into our C++ instance
void SoldierECDHHandler::handleLoRaDataStatic(const uint8_t* data, size_t len) {
    if (instance) {
        instance->handleLoRaData(data, len);
    }
}

void SoldierECDHHandler::handleLoRaData(const uint8_t* data, size_t len) {
    if (instance->hasResponded) return;

    // 1) Parse JSON & check recipient ID
    String msg((const char*)data, len);
    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, msg) != DeserializationError::Ok) {
        Serial.println(F("❌ JSON parse"));
        return;
    }
    int recipientId = doc["id"];
    if (recipientId != instance->config->getId()) {
        Serial.println(F("⚠️ Not for me"));
        return;
    }

    // 2) Base64→raw wrapped RSA key
    std::vector<uint8_t> wrappedKey;
    if (!CryptoHelper::decodeBase64(doc["key"].as<String>(), wrappedKey)) {
        Serial.println(F("❌ Wrapped-key Base64→bin failed"));
        return;
    }

    // 3) RSA-OAEP unwrap → symmetric key (using your 3-arg function)
    std::string symKeyStr;
    if (!instance->crypto.decryptWithPrivateKey(
            *instance->crypto.getPrivateKey(),  // mbedtls_pk_context&
            wrappedKey,                         // encrypted blob
            symKeyStr))                         // output plaintext
    {
        Serial.println(F("❌ unwrap sym key failed"));
        return;
    }
    Serial.printf("✅ Unwrapped sym key length: %u\n", (unsigned)symKeyStr.size());

    // 3b) convert to vector for AES
    std::vector<uint8_t> symKey(
        reinterpret_cast<uint8_t*>(symKeyStr.data()),
        reinterpret_cast<uint8_t*>(symKeyStr.data()) + symKeyStr.size()
    );

    // 4) Decode IV
    std::vector<uint8_t> iv;
    if (!CryptoHelper::decodeBase64(doc["iv"].as<String>(), iv) || iv.size() != 16) {
        Serial.println(F("❌ bad IV"));
        return;
    }
    Serial.printf("✅ IV length: %u\n", (unsigned)iv.size());

    // 5) AES-CTR decrypt helper (unchanged)
    auto aesCtrDecrypt = [&](const String& cipherB64,
                             std::vector<uint8_t>& outPlain)
    {
        std::vector<uint8_t> ct;
        CryptoHelper::decodeBase64(cipherB64, ct);

        mbedtls_cipher_context_t ctx;
        mbedtls_cipher_init(&ctx);
        const mbedtls_cipher_info_t* info =
            mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_CTR);
        mbedtls_cipher_setup(&ctx, info);
        mbedtls_cipher_setkey(&ctx,
                              symKey.data(), symKey.size()*8,
                              MBEDTLS_DECRYPT);
        mbedtls_cipher_set_iv(&ctx, iv.data(), iv.size());
        mbedtls_cipher_reset(&ctx);

        outPlain.resize(ct.size());
        size_t outl=0, finl=0;
        mbedtls_cipher_update(&ctx, ct.data(), ct.size(),
                              outPlain.data(), &outl);
        mbedtls_cipher_finish(&ctx, outPlain.data()+outl, &finl);
        outPlain.resize(outl + finl);
        mbedtls_cipher_free(&ctx);

        Serial.printf("   AES decrypt: %u→%u bytes\n",
                      (unsigned)ct.size(), (unsigned)outPlain.size());
    };

    // 6) Decrypt commander certificate (yields PEM text directly)
    std::vector<uint8_t> pemRaw;
    aesCtrDecrypt(doc["cert"].as<String>(), pemRaw);
    String certPem((char*)pemRaw.data(), pemRaw.size());

    // 7) Parse & verify PEM
    if (!instance->crypto.loadSingleCertificate(certPem) ||
        !instance->crypto.verifyCertificate())
    {
        Serial.println(F("❌ cert parse/verify"));
        return;
    }
    Serial.println(F("✅ Commander cert valid"));


    // // 8) Decrypt & import ephemeral ECDH key
    // std::vector<uint8_t> ephB64raw, ephRaw;
    // aesCtrDecrypt(doc["ephemeral"].as<String>(), ephB64raw);
    // String ephB64((char*)ephB64raw.data(), ephB64raw.size());
    // CryptoHelper::decodeBase64(ephB64, ephRaw);

    // if (!instance->ecdh.importPeerPublicKey(ephRaw) ||
    //     !instance->ecdh.deriveSharedSecret(instance->sharedSecret))
    // {
    //     Serial.println(F("❌ ECDH derive"));
    //     return;
    // }

   // ——— decrypt the ephemeral key blob ———
std::vector<uint8_t> ephRaw;
aesCtrDecrypt(doc["ephemeral"].as<String>(), ephRaw);
Serial.printf("   AES decrypt produced %u bytes of raw ephemeral data\n", (unsigned)ephRaw.size());

// generate soldier’s ephemeral
if (!instance->ecdh.generateEphemeralKey()) {
    Serial.println(F("❌ Soldier failed to generate ephemeral key"));
    return;
}
Serial.println(F("✅ Soldier ephemeral key generated"));

// import commander’s ephemeral
Serial.println("   Importing peer public key…");
if (!instance->ecdh.importPeerPublicKey(ephRaw)) {
    Serial.println(F("❌ importPeerPublicKey failed"));
    return;
}
Serial.println(F("   importPeerPublicKey succeeded"));

// derive the shared secret
std::vector<uint8_t> shared;
if (!instance->ecdh.deriveSharedSecret(shared)) {
    Serial.println(F("❌ deriveSharedSecret failed"));
    return;
}
instance->sharedSecret = std::move(shared);
Serial.println(F("✅ Shared secret OK"));


    instance->hasResponded = true;
    sendResponse(10); //TODO: change to the real commanders id
}





// void SoldierECDHHandler::sendResponse(int toId) {
//     Serial.println("📤 Sending soldier response...");
//     String cert = instance->config->getCertificatePEM();
//     String ephB64 = toBase64(instance->ecdh.getPublicKeyRaw());

//     DynamicJsonDocument doc(4096);
//     doc["id"] = instance->config->getId(); // change to commander id 
//     doc["cert"] = cert;
//     doc["ephemeral"] = ephB64;

//     String out;
//     serializeJson(doc, out);
//     if (instance->lora.sendFile((const uint8_t*)out.c_str(), out.length(), 180) != RADIOLIB_ERR_NONE) {
//         Serial.println("❌ Failed to send back message to the commander");
//     }
//     Serial.println("✅ Response sent");
// }

void SoldierECDHHandler::sendResponse(int toId) {
    Serial.println("📤 Preparing soldier response…");

    // 1) Ensure we have our ECDH ephemeral keypair
    if (!ecdh.generateEphemeralKey()) {
        Serial.println("❌ Failed to generate soldier ephemeral key");
        return;
    }

    // 2) Extract commander’s public key directly from the X.509 cert we loaded
    mbedtls_x509_crt* cmdCert = crypto.getCertificateCtx();
    mbedtls_pk_context*  cmdPk   = &cmdCert->pk;
    Serial.println("🔍 [DEBUG] Using commander’s public key from loaded certificate");

    // 3) Generate a fresh 256-bit AES key + 128-bit IV
    uint8_t symKey[32], iv[16];
    mbedtls_ctr_drbg_random(&crypto.getDrbg(), symKey, sizeof(symKey));
    mbedtls_ctr_drbg_random(&crypto.getDrbg(), iv,     sizeof(iv));

    // 4) Wrap that AES key under RSA-OAEP with the commander’s public key
    std::vector<uint8_t> wrappedKey(mbedtls_pk_get_len(cmdPk));
    size_t wrappedLen = 0;
    if (mbedtls_pk_encrypt(cmdPk,
                           symKey, sizeof(symKey),
                           wrappedKey.data(), &wrappedLen, wrappedKey.size(),
                           mbedtls_ctr_drbg_random, &crypto.getDrbg()) != 0)
    {
        Serial.println("❌ RSA wrap of AES key failed");
        return;
    }
    wrappedKey.resize(wrappedLen);

    // 5) AES-CTR encrypt helper (takes a Base64-encoded plaintext string)
    auto aesCtrEncrypt = [&](const String& plainB64,
                             std::vector<uint8_t>& out) {
        std::vector<uint8_t> plain;
        CryptoHelper::decodeBase64(plainB64, plain);

        mbedtls_cipher_context_t ctx;
        mbedtls_cipher_init(&ctx);
        const auto* info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_CTR);
        mbedtls_cipher_setup(&ctx, info);
        mbedtls_cipher_setkey(&ctx, symKey, 256, MBEDTLS_ENCRYPT);
        mbedtls_cipher_set_iv(&ctx, iv, sizeof(iv));
        mbedtls_cipher_reset(&ctx);

        out.resize(plain.size());
        size_t outl=0, finl=0;
        mbedtls_cipher_update(&ctx, plain.data(), plain.size(), out.data(), &outl);
        mbedtls_cipher_finish(&ctx, out.data()+outl, &finl);
        out.resize(outl + finl);

        mbedtls_cipher_free(&ctx);
    };

    // 6) Encrypt soldier’s cert & Base64-encoded ECDH public key
    std::vector<uint8_t> ctCert, ctEph;
    aesCtrEncrypt(config->getCertificatePEM(),      ctCert);
    aesCtrEncrypt(crypto.toBase64(ecdh.getPublicKeyRaw()), ctEph);

    // 7) Base64-encode wrapped key, IV, and ciphertexts
    String wrappedKeyB64 = CryptoHelper::toBase64(wrappedKey);
    String ivB64         = CryptoHelper::toBase64({ iv, iv + sizeof(iv) });
    String certCTB64     = CryptoHelper::toBase64(ctCert);
    String ephCTB64      = CryptoHelper::toBase64(ctEph);

    // 8) Build JSON body exactly like the commander’s
    DynamicJsonDocument doc(8192);
    doc["id"]        = toId;            // commander’s ID
    doc["key"]       = wrappedKeyB64;   // RSA-wrapped AES key
    doc["iv"]        = ivB64;           // AES IV
    doc["cert"]      = certCTB64;       // AES-encrypted cert
    doc["ephemeral"] = ephCTB64;         // AES-encrypted ECDH key

    String out;
    serializeJsonPretty(doc, out);

    // 9) Send via LoRa
    if (lora.sendFile((const uint8_t*)out.c_str(), out.length(), 180)
            != RADIOLIB_ERR_NONE) {
        Serial.println("❌ Failed to send soldier response");
    } else {
        Serial.println("✅ Soldier response sent");
    }
}



std::vector<uint8_t> SoldierECDHHandler::getSharedSecret() {
    if (!hasResponded) {
        Serial.println("⚠️ Shared secret not ready");
        return {};
    }
    return sharedSecret;
}

bool SoldierECDHHandler::hasRespondedToCommander() const {
    return hasResponded;
}

bool SoldierECDHHandler::decodeBase64(const String& input, std::vector<uint8_t>& output) {
    size_t inputLen = input.length();
    size_t decodedLen = (inputLen * 3) / 4 + 2;
    output.resize(decodedLen);
    size_t outLen = 0;
    int ret = mbedtls_base64_decode(output.data(), output.size(), &outLen,
                                    (const uint8_t*)input.c_str(), inputLen);
    if (ret != 0) return false;
    output.resize(outLen);
    return true;
}

String SoldierECDHHandler::toBase64(const std::vector<uint8_t>& input) {
    size_t outLen = 4 * ((input.size() + 2) / 3);
    std::vector<uint8_t> outBuf(outLen + 1);
    size_t actualLen = 0;
    int ret = mbedtls_base64_encode(outBuf.data(), outBuf.size(), &actualLen,
                                    input.data(), input.size());
    if (ret != 0) return "";
    outBuf[actualLen] = '\0';
    return String((const char*)outBuf.data());
}

void SoldierECDHHandler::poll() {
  lora.readData();
  lora.cleanUpTransmissions();
}
