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

CommanderECDHHandler* CommanderECDHHandler::instance = nullptr;

CommanderECDHHandler::CommanderECDHHandler(float freq, Commander* cmdr, certModule& crypt)
    : lora(freq), commander(cmdr), crypto(crypt), waitingResponse(false), hasHandled(false) {
    instance = this;
}

void CommanderECDHHandler::begin() {
    lora.setup(true);
    lora.setOnReadData([](const uint8_t* pkt, size_t len) {
        instance->lora.onLoraFileDataReceived(pkt, len);
    });
    //set the function called after reciving a file
    lora.setOnFileReceived(CommanderECDHHandler::handleLoRaDataStatic);
}

bool CommanderECDHHandler::startECDHExchange(int soldierId) {
    // 1) Generate our ECDH ephemeral
    if (!ecdh.generateEphemeralKey()) {
        Serial.println("❌ Failed to generate ephemeral key");
        return false;
    }

    // ────────────────────────────────────────────────────────────────────────────
    // 2) Load soldier’s Base64‐of‐PEM from JSON and decode to raw PEM
    String b64Pem = getPeerPublicKeyPEM(soldierId);  
    Serial.print  ("🔍 [DEBUG] JSON Base64‐of‐PEM: ");
    Serial.println(b64Pem);

    std::vector<uint8_t> pemBuf;
    if (!certModule::decodeBase64(b64Pem, pemBuf)) {
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
        certModule::decodeBase64(plainB64, plain);

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
    aesCtrEncrypt(getCertificatePEM(), ctCert);
    aesCtrEncrypt(crypto.toBase64(ecdh.getPublicKeyRaw()), ctEph);

    // 8) Base64-encode everything
    String wrappedKeyB64 = certModule::toBase64(wrappedKey);
    String ivB64         = certModule::toBase64({ iv, iv + sizeof(iv) });
    String certCTB64     = certModule::toBase64(ctCert);
    String ephCTB64      = certModule::toBase64(ctEph);

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
    serializeJson(doc, out);
    Serial.println(">>> RAW JSON >>>");
    Serial.println(out);

    if (lora.sendFile((const uint8_t*)out.c_str(), out.length(), 180)
            != RADIOLIB_ERR_NONE) {
        Serial.println("❌ Failed to send init message");
        return false;
    }

    Serial.println("📤 Init message with wrapped key sent");
    waitingResponse = true;
    startWait      = millis();

    //switch to reciver mode and wait for incoming packets
    // lv_timer_t* fileReceiveTimer = lv_timer_create([](lv_timer_t* t) {
    //     lora.handleCompletedOperation();
    //     if (!lora.isBusy()) {
    //         lora.readData();
    //     }
    // }, 100, nullptr);  // every 100ms


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



// ────────────────────────────────────────────────────────────────────────────
// static wrapper: bridges the C‐style callback into our C++ instance
void CommanderECDHHandler::handleLoRaDataStatic(const uint8_t* data, size_t len) {
    if (instance) {
        instance->handleLoRaData(data, len);
    }
}





void CommanderECDHHandler::handleLoRaData(const uint8_t* data, size_t len) {
    // only handle one response
    if (!waitingResponse || hasHandled) return;

    // Convert raw bytes to a String so we can print it
    String msg1((const char*)data, len);

    // ─── DEBUG: print the entire received JSON blob ───
    Serial.println(F("▶ Received raw JSON:"));
    Serial.println(msg1);


    // 1) Parse JSON & check recipient ID
    String msg((const char*)data, len);
    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, msg) != DeserializationError::Ok) {
        Serial.println(F("❌ JSON parse"));
        return;
    }

    //1.5) validate id
    int recipientId = doc["id"];
    if (recipientId != instance->commander->getCommanderNumber()) {
        Serial.println(F("⚠️ Not for me"));
        return;
    }

    // 2) Base64→raw wrapped RSA key
    std::vector<uint8_t> wrappedKey;
    if (!certModule::decodeBase64(doc["key"].as<String>(), wrappedKey)) {
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
    if (!certModule::decodeBase64(doc["iv"].as<String>(), iv) || iv.size() != 16) {
        Serial.println(F("❌ bad IV"));
        return;
    }
    Serial.printf("✅ IV length: %u\n", (unsigned)iv.size());

    // 5) AES-CTR decrypt helper (unchanged)
    auto aesCtrDecrypt = [&](const String& cipherB64,
                             std::vector<uint8_t>& outPlain)
    {
        std::vector<uint8_t> ct;
        certModule::decodeBase64(cipherB64, ct);

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
    if (!instance->ecdh.importPeerPublicKey(ephRaw)) {
        Serial.println(F("❌ importPeerPublicKey failed"));
        return;
    }
    Serial.println(F("importPeerPublicKey succeeded"));

    // derive the shared secret
    std::vector<uint8_t> shared;
    if (!instance->ecdh.deriveSharedSecret(shared)) {
        Serial.println(F("❌ deriveSharedSecret failed"));
        return;
    }
    instance->sharedSecret = std::move(shared);
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
  //lora.cleanUpTransmissions();
}


// Returns the public key (from the commander’s certificate) in PEM format
String CommanderECDHHandler::getPeerPublicKeyPEM(int soldierId) {
    // 1) grab a reference to the parsed certificate struct
    auto& crt = commander
      ->getSoldiers()
      .at(static_cast<uint8_t>(soldierId))
      .cert;

    // 2) get a pointer to its public-key context
    mbedtls_pk_context pk = crt.pk;

    // 3) buffer must be large enough for your key (e.g. 2048-bit RSA ≃ 450 bytes PEM)
    constexpr size_t PUBKEY_BUF_SIZE = 1600;
    unsigned char buf[PUBKEY_BUF_SIZE];

    int ret = mbedtls_pk_write_pubkey_pem(&pk, buf, PUBKEY_BUF_SIZE);
    if (ret != 0) {
        Serial.printf("Error writing pubkey PEM: -0x%04X\n", -ret);
        return String();
    }
    return String(reinterpret_cast<char*>(buf));
}

// Returns your own certificate in PEM format
String CommanderECDHHandler::getCertificatePEM() {
    // 1) grab a reference to your stored x509 struct
    auto& cert = commander->getPublicCert();  // assume this returns a reference
    //    if it returns by value, change it to return a const& in your class!

    // 2) DER → PEM encoder
    constexpr size_t CERT_PEM_BUF_SIZE = 2048;
    unsigned char buf[CERT_PEM_BUF_SIZE];
    size_t olen = 0;

    int ret = mbedtls_pem_write_buffer(
        "-----BEGIN CERTIFICATE-----\n",
        "-----END CERTIFICATE-----\n",
        cert.raw.p,
        cert.raw.len,
        buf,
        CERT_PEM_BUF_SIZE,
        &olen
    );
    if (ret != 0) {
        Serial.printf("Error writing cert PEM: -0x%04X\n", -ret);
        return String();
    }
    return String(reinterpret_cast<char*>(buf));
}
