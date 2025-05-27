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
    lora.onFileReceived = handleLoRaData;
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

void CommanderECDHHandler::handleLoRaData(const uint8_t* data, size_t len) {
    if (instance->hasHandled) {
        Serial.println("⚠️ Already handled response");
        return;
    }

    // 1) Turn the raw buffer into a String and print it for debug
    String msg((const char*)data, len);
    Serial.println("📥 Received soldier response:");
    Serial.println(msg);

    // 2) Parse JSON into a big enough document
    DynamicJsonDocument doc(16 * 1024);
    auto err = deserializeJson(doc, msg);
    if (err) {
        Serial.print("❌ JSON parse error: ");
        Serial.println(err.c_str());
        return;
    }

    // 3) Validate the sender ID
    

    // 4) **CERT** is already a PEM string → load it directly
  std::vector<uint8_t> certRaw;
  if (!decodeBase64(doc["cert"].as<String>(), certRaw)) {
    Serial.println("❌ Cert decode");
    return;
  }

  if (!instance->crypto.loadSingleCertificate(String((const char*)certRaw.data()))) {
    Serial.println("❌ Cert parse");
    return;
  }

  if (!instance->crypto.verifyCertificate()) {
    Serial.println("❌ Invalid cert");
    return;
  }

Serial.println("✅ Soldier cert valid");

    // 5) **EPHEMERAL** is raw Base64 → decode then import
    std::vector<uint8_t> ephRaw;
    if (!decodeBase64(doc["ephemeral"].as<String>(), ephRaw)) {
        Serial.println("❌ Ephemeral decode failed");
        return;
    }
    if (!instance->ecdh.importPeerPublicKey(ephRaw)) {
        Serial.println("❌ Ephemeral key import failed");
        return;
    }

    // 6) Derive the shared secret
    if (!instance->ecdh.deriveSharedSecret(instance->sharedSecret)) {
        Serial.println("❌ Shared secret derivation failed");
        return;
    }

    Serial.println("✅ Shared secret OK");
    instance->hasHandled     = true;
    instance->waitingResponse = false;
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
