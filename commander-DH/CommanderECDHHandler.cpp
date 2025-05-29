// CommanderECDHHandler.cpp
#include "CommanderECDHHandler.h"
#include <mbedtls/base64.h>
#include <mbedtls/error.h>
#include "crypto-helper.h"
#include <mbedtls/cipher.h>    


CommanderECDHHandler* CommanderECDHHandler::instance = nullptr;

CommanderECDHHandler::CommanderECDHHandler(float freq, CommanderConfigModule* cfg, CryptoHelper& crypt)
    : lora(freq), config(cfg), crypto(crypt), waitingResponse(false), hasHandled(false) {
    instance = this;
}

void CommanderECDHHandler::begin() {
    lora.setup(false);
    lora.setOnReadData([](const uint8_t* pkt, size_t len) {
        instance->lora.onLoraFileDataReceived(pkt, len);
    });
    lora.onFileReceived = CommanderECDHHandler::handleLoRaDataStatic;
}

bool CommanderECDHHandler::startECDHExchange(int soldierId) {
    // 1) Generate our ECDH ephemeral
    if (!ecdh.generateEphemeralKey()) {
        Serial.println("❌ Failed to generate ephemeral key");
        return false;
    }

    // ────────────────────────────────────────────────────────────────────────────
    // 2) Load soldier’s Base64‐of‐PEM from JSON and decode to raw PEM
    String b64Pem = config->getPeerPublicKeyPEM();  // JSON field “1”, “2”, etc.
    Serial.print  ("🔍 [DEBUG] JSON Base64‐of‐PEM: ");
    Serial.println(b64Pem);

    std::vector<uint8_t> pemBuf;
    if (!CryptoHelper::decodeBase64(b64Pem, pemBuf)) {
        Serial.println("❌ Failed to Base64‐decode JSON field into PEM");
        return false;
    }
    // Null‐terminate for MbedTLS
    pemBuf.push_back('\0');

    // Debug: show the PEM header we just recovered
    Serial.println("🔍 [DEBUG] decoded PEM header:");
    Serial.println((char*)pemBuf.data());  // should print "-----BEGIN PUBLIC KEY-----"

    // 3) Parse that PEM directly
    mbedtls_pk_context soldierPubKey;
    mbedtls_pk_init(&soldierPubKey);
    int ret = mbedtls_pk_parse_public_key(
        &soldierPubKey,
        pemBuf.data(),
        pemBuf.size()
    );
    if (ret != 0) {
        char err[128];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ parse_public_key(PEM) failed: -0x%04X (%s)\n", -ret, err);
        mbedtls_pk_free(&soldierPubKey);
        return false;
    }
    Serial.println("✅ Soldier RSA public key parsed");

    // ────────────────────────────────────────────────────────────────────────────
    // 4) Generate a 256-bit symmetric key + 128-bit IV
    uint8_t symKey[32], iv[16];
    mbedtls_ctr_drbg_random(&crypto.getDrbg(), symKey, sizeof(symKey));
    mbedtls_ctr_drbg_random(&crypto.getDrbg(), iv, sizeof(iv));

    // 5) AES-CTR encrypt a plaintext (Base64 string) → ciphertext
    auto aesCtrEncrypt = [&](const String& plainB64, std::vector<uint8_t>& out) {
        std::vector<uint8_t> plain;
        CryptoHelper::decodeBase64(plainB64, plain);

        mbedtls_cipher_context_t ctx;
        mbedtls_cipher_init(&ctx);
        auto info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_CTR);
        mbedtls_cipher_setup(&ctx, info);
        mbedtls_cipher_setkey(&ctx, symKey, 256, MBEDTLS_ENCRYPT);
        mbedtls_cipher_set_iv(&ctx, iv, sizeof(iv));
        mbedtls_cipher_reset(&ctx);

        size_t olen = 0;
        out.resize(plain.size());
        mbedtls_cipher_update(&ctx, plain.data(), plain.size(), out.data(), &olen);
        size_t finish_o = 0;
        mbedtls_cipher_finish(&ctx, out.data() + olen, &finish_o);
        out.resize(olen + finish_o);

        mbedtls_cipher_free(&ctx);
    };

    // 6) Wrap AES key with RSA-OAEP
    std::vector<uint8_t> wrappedKey(mbedtls_pk_get_len(&soldierPubKey));
    size_t wrappedLen = 0;
    if (mbedtls_pk_encrypt(&soldierPubKey,
                           symKey, sizeof(symKey),
                           wrappedKey.data(), &wrappedLen, wrappedKey.size(),
                           mbedtls_ctr_drbg_random, &crypto.getDrbg()) != 0)
    {
        Serial.println("❌ RSA wrap of symmetric key failed");
        mbedtls_pk_free(&soldierPubKey);
        return false;
    }
    wrappedKey.resize(wrappedLen);

    // 7) Encrypt the two payloads under AES-CTR
    std::vector<uint8_t> ctCert, ctEph;
    aesCtrEncrypt(config->getCertificatePEM(), ctCert);
    aesCtrEncrypt(crypto.toBase64(ecdh.getPublicKeyRaw()), ctEph);

    // 8) Base64-encode everything
    String wrappedKeyB64 = CryptoHelper::toBase64(wrappedKey);
    String ivB64         = CryptoHelper::toBase64({ iv, iv + sizeof(iv) });
    String certCTB64     = CryptoHelper::toBase64(ctCert);
    String ephCTB64      = CryptoHelper::toBase64(ctEph);

    // Cleanup the RSA context
    mbedtls_pk_free(&soldierPubKey);

    // ────────────────────────────────────────────────────────────────────────────
    // 9) Build JSON and send
    DynamicJsonDocument doc(8192);
    doc["id"]        = soldierId;
    doc["key"]       = wrappedKeyB64;
    doc["iv"]        = ivB64;
    doc["cert"]      = certCTB64;
    doc["ephemeral"] = ephCTB64;

    String out;
    serializeJsonPretty(doc, out);
    if (lora.sendFile((const uint8_t*)out.c_str(), out.length(), 180)
            != RADIOLIB_ERR_NONE) {
        Serial.println("❌ Failed to send init message");
        return false;
    }

    Serial.println("📤 Init message with wrapped key sent");
    waitingResponse = true;
    startWait      = millis();
    lora.setupListening();
    return true;
}


bool CommanderECDHHandler::isExchangeComplete() {
    return hasHandled;
}

std::vector<uint8_t> CommanderECDHHandler::getSharedSecret() {
    if (!hasHandled) {
        Serial.println("⚠️ Shared secret not ready. Call isExchangeComplete() to check.");
        return {};
    }
    return sharedSecret;
}



// void CommanderECDHHandler::handleLoRaData(const uint8_t* data, size_t len) {
//     // debug dump:
//     Serial.printf("▶️ Received %u bytes:\n", (unsigned)len);
//     for (size_t i = 0; i < len; i++) {
//         Serial.printf("%02X ", data[i]);
//         if ((i & 0x1F) == 0x1F) Serial.println();
//     }
//     Serial.println("\n— end dump —");

//     if (instance->hasHandled) {
//         Serial.println("⚠️ Already handled response");
//         return;
//     }

//     // 1) Raw → String → debug
//     String msg((const char*)data, len);
//     Serial.println("📥 Received soldier response:");
//     Serial.println(msg);

//     // 2) Parse JSON
//     DynamicJsonDocument doc(16 * 1024);
//     auto err = deserializeJson(doc, msg);
//     if (err) {
//         Serial.print("❌ JSON parse error: ");
//         Serial.println(err.c_str());
//         return;
//     }

//     // 3) (optional) validate sender ID
//     int senderId = doc["id"];
//     // if (senderId != expectedSoldierId) { … }

//     // 4) Base64→raw wrapped AES key
//     std::vector<uint8_t> wrappedKey;
//     if (!CryptoHelper::decodeBase64(doc["key"].as<String>(), wrappedKey)) {
//         Serial.println("❌ Wrapped-key Base64→bin failed");
//         return;
//     }
//     Serial.printf("🔍 [DEBUG] wrappedKey length=%u bytes\n", (unsigned)wrappedKey.size());

//     // 5) RSA‐PKCS#1 v1.5 unwrap → symmetric key
//     mbedtls_pk_context*  pk  = instance->crypto.getPrivateKey();
//     mbedtls_rsa_context* rsa = mbedtls_pk_rsa(*pk);
//     mbedtls_rsa_set_padding(rsa, MBEDTLS_RSA_PKCS_V15, 0);

//     size_t plen = 0;
//     // decrypt into this buffer
//     std::vector<uint8_t> plain(mbedtls_pk_get_len(pk));
//     int ret = mbedtls_rsa_pkcs1_decrypt(
//         rsa,
//         mbedtls_ctr_drbg_random, &instance->crypto.getDrbg(),
//         MBEDTLS_RSA_PRIVATE,
//         &plen,
//         wrappedKey.data(),             // ciphertext
//         plain.data(), plain.size()     // output buffer
//     );
//     if (ret != 0) {
//         Serial.println("❌ RSA-PKCS#1 decrypt failed");
//         return;
//     }
//     plain.resize(plen);
//     Serial.printf("✅ Unwrapped sym key length: %u\n", (unsigned)plen);

//     // Move decrypted bytes into symKey exactly once
//     std::vector<uint8_t> symKey(plain.begin(), plain.end());

//     // 6) Base64→IV
//     std::vector<uint8_t> iv;
//     if (!CryptoHelper::decodeBase64(doc["iv"].as<String>(), iv) || iv.size() != 16) {
//         Serial.println("❌ IV decode failed");
//         return;
//     }
//     Serial.printf("✅ IV length: %u bytes\n", (unsigned)iv.size());

//     // 7) AES-CTR decrypt helper
//     auto aesCtrDecrypt = [&](const String& cipherB64, std::vector<uint8_t>& outPlain) {
//         std::vector<uint8_t> ct;
//         if (!CryptoHelper::decodeBase64(cipherB64, ct)) {
//             Serial.println("❌ AES-CTR Base64 decode failed");
//             return;
//         }

//         mbedtls_cipher_context_t ctx;
//         mbedtls_cipher_init(&ctx);
//         auto info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_CTR);
//         mbedtls_cipher_setup(&ctx, info);
//         mbedtls_cipher_setkey(&ctx, symKey.data(), symKey.size()*8, MBEDTLS_DECRYPT);
//         mbedtls_cipher_set_iv(&ctx, iv.data(), iv.size());
//         mbedtls_cipher_reset(&ctx);

//         outPlain.resize(ct.size());
//         size_t outl = 0, finl = 0;
//         mbedtls_cipher_update(&ctx, ct.data(), ct.size(), outPlain.data(), &outl);
//         mbedtls_cipher_finish(&ctx, outPlain.data()+outl, &finl);
//         outPlain.resize(outl + finl);
//         mbedtls_cipher_free(&ctx);

//         Serial.printf("   AES decrypt: %u→%u bytes\n",
//                       (unsigned)ct.size(), (unsigned)outPlain.size());
//     };

//     // 8) Decrypt & verify soldier’s certificate
//     std::vector<uint8_t> certRaw;
//     aesCtrDecrypt(doc["cert"].as<String>(), certRaw);
//     String certPem((char*)certRaw.data(), certRaw.size());
//     if (!instance->crypto.loadSingleCertificate(certPem) ||
//         !instance->crypto.verifyCertificate())
//     {
//         Serial.println("❌ Soldier cert parse/verify failed");
//         return;
//     }
//     Serial.println("✅ Soldier cert valid");

//     // 9) Decrypt & import soldier’s ephemeral ECDH public key
//     std::vector<uint8_t> ephRaw;
//     aesCtrDecrypt(doc["ephemeral"].as<String>(), ephRaw);
//     Serial.printf("🔍 [DEBUG] ephRaw length=%u bytes\n", (unsigned)ephRaw.size());
//     if (!instance->ecdh.importPeerPublicKey(ephRaw)) {
//         Serial.println("❌ Ephemeral key import failed");
//         return;
//     }

//     // 10) Derive the shared secret
//     if (!instance->ecdh.deriveSharedSecret(instance->sharedSecret)) {
//         Serial.println("❌ Shared secret derivation failed");
//         return;
//     }

//     Serial.println("✅ Shared secret OK");
//     instance->hasHandled     = true;
//     instance->waitingResponse = false;
// }






// ────────────────────────────────────────────────────────────────────────────
// static wrapper: bridges the C‐style callback into our C++ instance
void CommanderECDHHandler::handleLoRaDataStatic(const uint8_t* data, size_t len) {
    if (instance) {
        instance->handleLoRaData(data, len);
    }
}






// void CommanderECDHHandler::handleLoRaData(const uint8_t* data, size_t len) {
//     // only handle one response
//     if (!waitingResponse || hasHandled) return;

//     // rebuild JSON string
//     String msg((const char*)data, len);
//     if (!msg.endsWith("\n")) msg += "\n";
//     Serial.println(F("📥 Received soldier response:"));
//     Serial.println(msg);

//     // 1) parse JSON
//     DynamicJsonDocument doc(8192);
//     if (deserializeJson(doc, msg) != DeserializationError::Ok) {
//         Serial.println(F("❌ JSON parse failed"));
//         return;
//     }
//     String b64Key  = doc["key"].as<String>();
//     String b64Iv   = doc["iv"].as<String>();
//     String b64Cert = doc["cert"].as<String>();
//     String b64Eph  = doc["ephemeral"].as<String>();

//     // 2) Base64 → raw
//     std::vector<uint8_t> wrappedKey, iv, ctCert, ctEph;
//     CryptoHelper::decodeBase64(b64Key,  wrappedKey);
//     CryptoHelper::decodeBase64(b64Iv,   iv);
//     CryptoHelper::decodeBase64(b64Cert, ctCert);
//     CryptoHelper::decodeBase64(b64Eph,  ctEph);

//     // debug: make sure sizes match
//     Serial.printf("🔑 wrappedKey=%u bytes, iv=%u bytes, ctCert=%u bytes, ctEph=%u bytes\n",
//                   (unsigned)wrappedKey.size(),
//                   (unsigned)iv.size(),
//                   (unsigned)ctCert.size(),
//                   (unsigned)ctEph.size());

//     // 3) RSA-OAEP unwrap using *your* private key
//     uint8_t symKey[32];
//     size_t olen = 0;
//     mbedtls_pk_context* priv = crypto.getPrivateKey();
//     int rc = mbedtls_pk_decrypt(priv,
//                                 wrappedKey.data(), wrappedKey.size(),
//                                 symKey, &olen, sizeof(symKey),
//                                 mbedtls_ctr_drbg_random,
//                                 &crypto.getDrbg());
//     if (rc != 0 || olen != sizeof(symKey)) {
//         char err[100];
//         mbedtls_strerror(rc, err, sizeof(err));
//         Serial.printf("❌ RSA unwrap failed (-0x%04X): %s\n", -rc, err);
//         return;
//     }
//     Serial.println(F("✅ Unwrapped AES key"));

//     // 4) AES-CTR decrypt helper
//     auto aesCtrDecrypt = [&](const std::vector<uint8_t>& ct,
//                              std::vector<uint8_t>& pt) {
//         mbedtls_cipher_context_t ctx;
//         mbedtls_cipher_init(&ctx);
//         auto info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_CTR);
//         mbedtls_cipher_setup(&ctx, info);
//         mbedtls_cipher_setkey(&ctx, symKey, 256, MBEDTLS_DECRYPT);
//         mbedtls_cipher_set_iv(&ctx, iv.data(), iv.size());
//         mbedtls_cipher_reset(&ctx);

//         pt.resize(ct.size());
//         size_t outl = 0, finl = 0;
//         mbedtls_cipher_update(&ctx, ct.data(), ct.size(), pt.data(), &outl);
//         mbedtls_cipher_finish(&ctx, pt.data() + outl, &finl);
//         pt.resize(outl + finl);
//         mbedtls_cipher_free(&ctx);
//     };

//     // 5) decrypt the soldier’s cert & ephemeral blob
//     std::vector<uint8_t> ptCert, ptEphRaw;
//     aesCtrDecrypt(ctCert, ptCert);
//     aesCtrDecrypt(ctEph,  ptEphRaw);

//     // 6) load & verify the soldier’s certificate
//     String pemCert((char*)ptCert.data(), ptCert.size());
//     if (!crypto.loadSingleCertificate(pemCert) ||
//         !crypto.verifyCertificate())
//     {
//         Serial.println(F("❌ Soldier cert parse/verify failed"));
//         return;
//     }
//     Serial.println(F("✅ Soldier cert valid"));

//     // 7) Base64→bytes of the *decrypted* ephemeral blob
//     String b64EphRaw((char*)ptEphRaw.data(), ptEphRaw.size());
//     std::vector<uint8_t> ephRaw;
//     CryptoHelper::decodeBase64(b64EphRaw, ephRaw);

//     // 8) import & derive the ECDH shared secret
//     if (!ecdh.importPeerPublicKey(ephRaw)) {
//         Serial.println(F("❌ importPeerPublicKey failed"));
//         return;
//     }
//     std::vector<uint8_t> shared;
//     if (!ecdh.deriveSharedSecret(shared)) {
//         Serial.println(F("❌ deriveSharedSecret failed"));
//         return;
//     }
//     sharedSecret = std::move(shared);
//     Serial.println(F("✅ Shared secret OK"));

//     // done
//     hasHandled     = true;
//     waitingResponse = false;
// }






















void CommanderECDHHandler::handleLoRaData(const uint8_t* data, size_t len) {
    // only handle one response
    if (!waitingResponse || hasHandled) return;

    // 1) Parse JSON & check recipient ID
    String msg((const char*)data, len);
    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, msg) != DeserializationError::Ok) {
        Serial.println(F("❌ JSON parse"));
        return;
    }

    // 1.5) validate id - TODO: implement this.
    // int recipientId = doc["id"];
    // if (recipientId != instance->config->getId()) {
    //     Serial.println(F("⚠️ Not for me"));
    //     return;
    // }

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
    Serial.println(F("✅ Soldier cert valid"));

    // ——— decrypt the ephemeral key blob ———
    std::vector<uint8_t> ephRaw;
    aesCtrDecrypt(doc["ephemeral"].as<String>(), ephRaw);
    Serial.printf("   AES decrypt produced %u bytes of raw ephemeral data\n", (unsigned)ephRaw.size());

    // 8) import & derive the ECDH shared secret
    // import soldier’s ephemeral
    Serial.println("   Importing soldier public key…");
    if (!ecdh.importPeerPublicKey(ephRaw)) {
        Serial.println(F("❌ importPeerPublicKey failed"));
        return;
    }
    Serial.println(F("   importPeerPublicKey succeeded"));

    // derive the shared secret
    std::vector<uint8_t> shared;
    if (!ecdh.deriveSharedSecret(shared)) {
        Serial.println(F("❌ deriveSharedSecret failed"));
        return;
    }
    sharedSecret = std::move(shared);
    Serial.println(F("✅ Shared secret OK"));

    // done
    hasHandled     = true;
    waitingResponse = false;
}






















String CommanderECDHHandler::toBase64(const std::vector<uint8_t>& input) {
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

bool CommanderECDHHandler::decodeBase64(const String& input,
                                        std::vector<uint8_t>& output) {
    Serial.println("enter: decode function");
  size_t inputLen = input.length();
  size_t decodedLen = (inputLen * 3) / 4 + 2;
  output.resize(decodedLen);
  size_t outLen = 0;
  int ret = mbedtls_base64_decode(output.data(), output.size(), &outLen,
                                   (const uint8_t*)input.c_str(), inputLen);
  if (ret != 0) {
    char errBuf[128];
    mbedtls_strerror(ret, errBuf, sizeof(errBuf));
    Serial.printf("❌ Base64 decode failed: -0x%04X (%s)\n", -ret, errBuf);
    return false;
  }

  output.resize(outLen);
  return true;
}


void CommanderECDHHandler::poll() {
  lora.readData();
  lora.cleanUpTransmissions();
}
