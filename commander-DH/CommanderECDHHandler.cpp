// CommanderECDHHandler.cpp
#include "CommanderECDHHandler.h"
#include <mbedtls/base64.h>
#include <mbedtls/error.h>
#include "../certModule/certModule.h"
#include <mbedtls/cipher.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pem.h>

lv_timer_t* fileReceiveTimer;
int currentSoldierId = 0;

CommanderECDHHandler* CommanderECDHHandler::instance = nullptr;

CommanderECDHHandler::CommanderECDHHandler(float freq, Commander* cmdr, certModule& crypt)
    : lora(freq), commander(cmdr), crypto(crypt), waitingResponse(false), hasHandled(false) {
    instance = this;
}

void CommanderECDHHandler::begin() {
    lora.setup(/*transmissionMode=*/true);
    // hook up the low-level chunk callback
    lora.setOnReadData([](const uint8_t* pkt, size_t len) {
        instance->lora.onLoraFileDataReceived(pkt, len);
    });
    // when the file is fully reassembled, call our static handler
    lora.setOnFileReceived(CommanderECDHHandler::handleLoRaDataStatic);
}


bool CommanderECDHHandler::startECDHExchange(int soldierId) {
    currentSoldierId = soldierId;

    // 1) Generate our ECDH ephemeral
    if (!ecdh.generateEphemeralKey()) {
        Serial.println("❌ Failed to generate ephemeral key");
        return false;
    }

    // ────────────────────────────────────────────────────────────────────────────
        // ── 2) EXTRACT soldier’s public-key DER (not the full cert)
    const auto& cert = commander->getSoldiers().at(soldierId).cert;
    constexpr size_t PUB_DER_BUF = 512;
    unsigned char derBuf[PUB_DER_BUF];
    int pubDerLen = mbedtls_pk_write_pubkey_der(
        const_cast<mbedtls_pk_context*>(&cert.pk),
        derBuf, PUB_DER_BUF
    );
    if (pubDerLen < 0) {
      Serial.printf("❌ pubkey DER extract failed: -0x%04X\n", -pubDerLen);
      return false;
    }
    unsigned char* derStart = derBuf + (PUB_DER_BUF - pubDerLen);
    std::vector<uint8_t> pubDer(derStart, derStart + pubDerLen);

    // ── 3) Parse that DER directly into an mbedtls_pk_context
    mbedtls_pk_context soldierPubKey;
    mbedtls_pk_init(&soldierPubKey);
    int ret = mbedtls_pk_parse_public_key(
        &soldierPubKey,
        pubDer.data(),
        pubDer.size()
    );
    if (ret != 0) {
      char err[128];
      mbedtls_strerror(ret, err, sizeof(err));
      Serial.printf("❌ parse_public_key(DER) failed: -0x%04X (%s)\n", -ret, err);
      mbedtls_pk_free(&soldierPubKey);
      return false;
    }
    Serial.println("✅ Soldier RSA public key parsed (DER)");


    // ────────────────────────────────────────────────────────────────────────────
    // 4) Generate symmetric key + IV
    uint8_t symKey[32], iv[16];
    mbedtls_ctr_drbg_random(&crypto.getDrbg(), symKey, sizeof(symKey));
    mbedtls_ctr_drbg_random(&crypto.getDrbg(), iv, sizeof(iv));

    // 5) Wrap AES key with RSA-OAEP
    std::vector<uint8_t> wrappedKey(mbedtls_pk_get_len(&soldierPubKey));
    size_t wrappedLen = 0;
    if (mbedtls_pk_encrypt(&soldierPubKey,
                           symKey, sizeof(symKey),
                           wrappedKey.data(), &wrappedLen, wrappedKey.size(),
                           mbedtls_ctr_drbg_random, &crypto.getDrbg()) != 0)
    {
        Serial.println("❌ RSA wrap failed");
        mbedtls_pk_free(&soldierPubKey);
        return false;
    }
    wrappedKey.resize(wrappedLen);

    // 6) AES-CTR encrypt helper (captures symKey+iv)
    auto aesCtrEncryptRaw = [&](const std::vector<uint8_t>& plain,
                                std::vector<uint8_t>& out) {
        mbedtls_cipher_context_t ctx;
        mbedtls_cipher_init(&ctx);
        const mbedtls_cipher_info_t* info =
            mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_CTR);
        mbedtls_cipher_setup(&ctx, info);
        mbedtls_cipher_setkey(&ctx, symKey, 256, MBEDTLS_ENCRYPT);
        mbedtls_cipher_set_iv(&ctx, iv, sizeof(iv));
        mbedtls_cipher_reset(&ctx);

        size_t olen = 0, finish_len = 0;
        out.resize(plain.size());
        mbedtls_cipher_update(&ctx, plain.data(), plain.size(), out.data(), &olen);
        mbedtls_cipher_finish(&ctx, out.data() + olen, &finish_len);
        out.resize(olen + finish_len);

        mbedtls_cipher_free(&ctx);
    };

    // 7) Encrypt our own DER cert + DER ECDH pubkey
    std::vector<uint8_t> derCert(cert.raw.p, cert.raw.p + cert.raw.len);
    std::vector<uint8_t> derEph = ecdh.getPublicKeyRaw();
    std::vector<uint8_t> ctCert, ctEph;
    aesCtrEncryptRaw(derCert, ctCert);
    aesCtrEncryptRaw(derEph,  ctEph);

    // 8) Base64-encode everything
    String wrappedKeyB64 = certModule::toBase64(wrappedKey);
    String ivB64         = certModule::toBase64({ iv, iv + sizeof(iv) });
    String certCTB64     = certModule::toBase64(ctCert);
    String ephCTB64      = certModule::toBase64(ctEph);

    // Cleanup
    mbedtls_pk_free(&soldierPubKey);

    // 9) Build & send JSON
    DynamicJsonDocument doc(8192);
    doc["id"]        = soldierId;
    doc["key"]       = wrappedKeyB64;
    doc["iv"]        = ivB64;
    doc["cert"]      = certCTB64;
    doc["ephemeral"] = ephCTB64;

    String out;
    serializeJson(doc, out);

    if (lora.sendFile((const uint8_t*)out.c_str(), out.length(), 180)
            != RADIOLIB_ERR_NONE) {
        Serial.println("❌ Failed to send init message");
        return false;
    }

    Serial.println("📤 Init message with wrapped key sent");
    waitingResponse = true;
    startWait = millis();

    // ────────────────────────────────────────────────────────────────────
    // **Immediately** switch back into RX mode so we catch the reply:
    lora.setup(/* transmissionMode = */ false);
    lora.readData();

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



// // ────────────────────────────────────────────────────────────────────────────
// // static wrapper: bridges the C‐style callback into our C++ instance
// static bridge — exactly as Soldier does
void CommanderECDHHandler::handleLoRaDataStatic(const uint8_t* data, size_t len) {
    if (instance) {
        instance->handleLoRaData(data, len);
    }
}

void CommanderECDHHandler::handleLoRaData(const uint8_t* data, size_t len) {
    // 1) Stop the receive‐timer
    if(fileReceiveTimer) {
        lv_timer_del(fileReceiveTimer);
        fileReceiveTimer = nullptr;
    }

    // 2) Only handle the first valid response
    if (!waitingResponse || hasHandled) return;

    // 3) Read and parse JSON
    String msg((const char*)data, len);
    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, msg) != DeserializationError::Ok) {
        Serial.println("❌ JSON parse");
        return;
    }

    // 4) Ensure it’s addressed to *this* commander
    int recipientId = doc["id"];
    if (recipientId != commander->getCommanderNumber()) {
        Serial.println("⚠️ Not for me");
        return;
    }

    // 5) RSA-OAEP unwrap the AES key
    std::vector<uint8_t> wrappedKey;
    if (!certModule::decodeBase64(doc["key"].as<String>(), wrappedKey)) {
        Serial.println("❌ Wrapped-key Base64→bin failed");
        return;
    }
    std::string symKeyStr;
    if (!crypto.decryptWithPrivateKey(
            *crypto.getPrivateKey(),
            wrappedKey,
            symKeyStr))
    {
        Serial.println("❌ unwrap sym key failed");
        return;
    }
    Serial.printf("✅ Unwrapped sym key length: %u\n", (unsigned)symKeyStr.size());
    std::vector<uint8_t> symKey(symKeyStr.begin(), symKeyStr.end());

    // 6) Decode IV
    std::vector<uint8_t> iv;
    if (!certModule::decodeBase64(doc["iv"].as<String>(), iv) || iv.size() != 16) {
        Serial.println("❌ bad IV");
        return;
    }
    Serial.printf("✅ IV length: %u\n", (unsigned)iv.size());

    // 7) AES-CTR decrypt helper
    auto aesCtrDecryptRaw = [&](const std::vector<uint8_t>& ct,
                                std::vector<uint8_t>& outPlain)
    {
        mbedtls_cipher_context_t ctx;
        mbedtls_cipher_init(&ctx);
        const mbedtls_cipher_info_t* info =
            mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_CTR);
        mbedtls_cipher_setup(&ctx, info);
        mbedtls_cipher_setkey(&ctx, symKey.data(), symKey.size()*8, MBEDTLS_DECRYPT);
        mbedtls_cipher_set_iv(&ctx, iv.data(), iv.size());
        mbedtls_cipher_reset(&ctx);

        size_t outl=0, finl=0;
        outPlain.resize(ct.size());
        mbedtls_cipher_update(&ctx, ct.data(), ct.size(), outPlain.data(), &outl);
        mbedtls_cipher_finish(&ctx, outPlain.data()+outl, &finl);
        outPlain.resize(outl + finl);
        mbedtls_cipher_free(&ctx);
    };

    // 8) Decrypt & parse Soldier’s certificate (DER↓PEM skipped, DER only)
    std::vector<uint8_t> ctCert, derCert;
    certModule::decodeBase64(doc["cert"].as<String>(), ctCert);
    aesCtrDecryptRaw(ctCert, derCert);

    mbedtls_x509_crt peerCert;
    mbedtls_x509_crt_init(&peerCert);
    if (mbedtls_x509_crt_parse_der(&peerCert, derCert.data(), derCert.size()) != 0) {
        Serial.println("❌ peer cert parse DER failed");
        return;
    }

    // 9) Verify cert against our CA
    if (!certModule::verifyCertificate(
            &peerCert,
            const_cast<mbedtls_x509_crt*>(&commander->getCAPublicCert())))
    {
        Serial.println("❌ cert verify failed");
        mbedtls_x509_crt_free(&peerCert);
        return;
    }
    Serial.println("✅ Soldier cert valid");
    mbedtls_x509_crt_free(&peerCert);

    // 10) Decrypt & recover Soldier’s ephemeral public key (raw X||Y)
    std::vector<uint8_t> ctEph, derEph;
    certModule::decodeBase64(doc["ephemeral"].as<String>(), ctEph);
    aesCtrDecryptRaw(ctEph, derEph);

    if (derEph.empty()) {
        Serial.println("❌ derEph is empty – nothing to import!");
        return;
    }
    // if it came back without the 0x04 prefix, add it
    if (derEph.size() == 64 && derEph[0] != 0x04) {
        Serial.println("⚠️ prefixing 0x04 to uncompressed point");
        derEph.insert(derEph.begin(), 0x04);
    }

    // 11) Import into our ECDH helper & derive the shared secret
    if (!ecdh.importPeerPublicKey(derEph)) {
        Serial.println("❌ importPeerPublicKey failed");
        return;
    }
    Serial.println("✅ imported soldier ephemeral key");

    std::vector<uint8_t> shared;
    if (!ecdh.deriveSharedSecret(shared)) {
        Serial.println("❌ deriveSharedSecret failed");
        return;
    }
    sharedSecret = std::move(shared);
    Serial.println("✅ Shared secret OK");

    // 12) Done
    hasHandled     = true;
    waitingResponse = false;

    // send the final message encrypted with the shared secret
    sendSecureMessage(currentSoldierId, "hello");
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
    // 1) Service any completed RX/TX interrupt & dispatch callbacks
    lora.handleCompletedOperation();

    // 2) If we’re not in the middle of anything, re-arm for receive
    if (!lora.isBusy()) {
        lora.setup(/* transmissionMode = */ false);
        lora.readData();
    }
}


bool CommanderECDHHandler::sendSecureMessage(int soldierId, const String& plaintext) {
    if (!hasHandled) {
        Serial.println("❌ Handshake not complete");
        return false;
    }

    // 1) Turn plaintext into bytes
    std::vector<uint8_t> pt(plaintext.begin(), plaintext.end());

    // 2) Generate 128-bit IV
    std::vector<uint8_t> iv(16);
    mbedtls_ctr_drbg_random(&crypto.getDrbg(), iv.data(), iv.size());

    // 3) AES-256-CTR encrypt using sharedSecret as key
    mbedtls_cipher_context_t ctx;
    mbedtls_cipher_init(&ctx);
    const mbedtls_cipher_info_t* info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_CTR);
    mbedtls_cipher_setup(&ctx, info);
    mbedtls_cipher_setkey(&ctx, sharedSecret.data(), 256, MBEDTLS_ENCRYPT);
    mbedtls_cipher_set_iv(&ctx, iv.data(), iv.size());
    mbedtls_cipher_reset(&ctx);

    std::vector<uint8_t> ct(pt.size());
    size_t olen = 0, finl = 0;
    mbedtls_cipher_update(&ctx, pt.data(), pt.size(), ct.data(), &olen);
    mbedtls_cipher_finish(&ctx, ct.data() + olen, &finl);
    ct.resize(olen + finl);
    mbedtls_cipher_free(&ctx);

    // 4) Base64-encode IV + ciphertext
    String ivB64  = toBase64(iv);
    String msgB64 = toBase64(ct);

    // 5) Build JSON envelope
    DynamicJsonDocument doc(256);
    doc["id"]  = soldierId;
    doc["iv"]  = ivB64;
    doc["msg"] = msgB64;
    String out;
    serializeJson(doc, out);

    // 6) Kick off the sendFile()
    if (lora.sendFile((const uint8_t*)out.c_str(), out.length(), 180) != RADIOLIB_ERR_NONE) {
        Serial.println("❌ Failed to send secure msg");
        return false;
    }
    Serial.println("📤 Secure message scheduled");

    // ────────────────────────────────────────────────────────────
    // 7) Drive the TX until complete via a 100 ms timer
    fileReceiveTimer = lv_timer_create(
        [](lv_timer_t* t) {
            auto* self = static_cast<CommanderECDHHandler*>(t->user_data);
            // service the LoRa module
            self->lora.handleCompletedOperation();
            // once all chunks + FILE_END are on-air, tear down the timer
            if (!self->lora.isBusy()) {
                lv_timer_del(t);
                Serial.println("✅ Secure message fully sent");
            }
        },
        100,
        this
    );

    return true;
}
