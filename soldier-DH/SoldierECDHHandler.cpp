// SoldierECDHHandler.cpp
#include "SoldierECDHHandler.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"
#include <mbedtls/error.h>
#include "../certModule/certModule.h"
#include "mbedtls/ctr_drbg.h"
 #include "mbedtls/entropy.h"
#include <mbedtls/cipher.h>
#include <mbedtls/pk.h>
#include <mbedtls/x509_crt.h>
#include <mbedtls/pem.h>

lv_timer_t* fileReceiveTimer;

SoldierECDHHandler* SoldierECDHHandler::instance = nullptr;

SoldierECDHHandler::SoldierECDHHandler(float freq, Soldier* soldier, certModule& crypt)
    : lora(freq), soldier(soldier), crypto(crypt), hasResponded(false) {
    instance = this;
}


void SoldierECDHHandler::begin() {
    instance = this;
    lora.setup(false);
    lora.setOnReadData([](const uint8_t* pkt, size_t len) {
        instance->lora.onLoraFileDataReceived(pkt, len);
    });
    //set the function called after reciving a file
    lora.setOnFileReceived(SoldierECDHHandler::handleLoRaDataStatic);
}


void SoldierECDHHandler::startListening() {
  hasResponded = false;
  hasReceiveMessage=false;
  lora.setup(false);
  lora.readData();    // <-- first receive
  fileReceiveTimer = lv_timer_create(
    [](lv_timer_t* t) {
      auto* self = static_cast<SoldierECDHHandler*>(t->user_data);
      self->lora.handleCompletedOperation();
      self->lora.readData();  // <-- always re-arm
    },
    100,
    this
  );
}





// ────────────────────────────────────────────────────────────────────────────
// static wrapper: bridges the C‐style callback into our C++ instance
void SoldierECDHHandler::handleLoRaDataStatic(const uint8_t* data, size_t len) {
    Serial.println("entered handle data function");

    if (instance) {
        instance->handleLoRaData(data, len);
    }
}

void SoldierECDHHandler::handleLoRaData(const uint8_t* data, size_t len) {
    // stop our receive‐timer
    lv_timer_del(fileReceiveTimer);
    fileReceiveTimer = nullptr;

    if (hasResponded) return;

    // 1) Parse JSON
    String msg((const char*)data, len);
    DynamicJsonDocument doc(8192);
    if (deserializeJson(doc, msg) != DeserializationError::Ok) {
        Serial.println("❌ JSON parse");
        return;
    }

    // 2) Check it’s addressed to me
    int recipientId = doc["id"];
    if (recipientId != soldier->getSoldierNumber()) {
        Serial.println("⚠️ Not for me");
        return;
    }

    // 3) Unwrap the symmetric key (RSA-OAEP)
    std::vector<uint8_t> wrappedKey;
    certModule::decodeBase64(doc["key"].as<String>(), wrappedKey);
    std::string symKeyStr;
    if (!crypto.decryptWithPrivateKey(
            soldier->getPrivateKey(),
            wrappedKey,
            symKeyStr)) {
        Serial.println("❌ unwrap sym key failed");
        return;
    }
    std::vector<uint8_t> symKey(
        symKeyStr.begin(), symKeyStr.end()
    );
    Serial.printf("✅ Unwrapped sym key: %u bytes\n", (unsigned)symKey.size());

    // 4) Decode IV
    std::vector<uint8_t> iv;
    certModule::decodeBase64(doc["iv"].as<String>(), iv);
    if (iv.size() != 16) {
        Serial.println("❌ bad IV");
        return;
    }

    // 5) AES-CTR decrypt helper (raw DER)
    auto aesCtrDecryptRaw = [&](const std::vector<uint8_t>& ct,
                                std::vector<uint8_t>& outPlain) {
        mbedtls_cipher_context_t ctx;
        mbedtls_cipher_init(&ctx);
        auto info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_CTR);
        mbedtls_cipher_setup(&ctx, info);
        mbedtls_cipher_setkey(&ctx, symKey.data(), 256, MBEDTLS_DECRYPT);
        mbedtls_cipher_set_iv(&ctx, iv.data(), iv.size());
        mbedtls_cipher_reset(&ctx);

        size_t outl=0, finl=0;
        outPlain.resize(ct.size());
        mbedtls_cipher_update(&ctx, ct.data(), ct.size(), outPlain.data(), &outl);
        mbedtls_cipher_finish(&ctx, outPlain.data()+outl, &finl);
        outPlain.resize(outl + finl);
        mbedtls_cipher_free(&ctx);
    };

    // 6) Decrypt & parse commander’s certificate (DER)
    std::vector<uint8_t> ctCert, derCert;
    certModule::decodeBase64(doc["cert"].as<String>(), ctCert);
    aesCtrDecryptRaw(ctCert, derCert);

    mbedtls_x509_crt peerCert;
    mbedtls_x509_crt_init(&peerCert);
    if (mbedtls_x509_crt_parse_der(&peerCert, derCert.data(), derCert.size()) != 0) {
        Serial.println("❌ peer cert parse DER failed");
        return;
    }

    // 7) Verify it against our CA
    if (!certModule::verifyCertificate(
          &peerCert,
          const_cast<mbedtls_x509_crt*>(&soldier->getCAPublicCert())
        )) {
        Serial.println("❌ cert verify failed");
        return;
    }
    Serial.println("✅ Commander cert valid");

    //7.5) generate own ephemeral key pair
    // …after decrypting the commander’s envelope…
if (!ecdh.generateEphemeralKey()) {
  Serial.println("❌ Soldier failed to generate ephemeral key");
  return;
}

    // 8) Decode & import commander’s ephemeral public key (DER)
String ephB64 = doc["ephemeral"].as<String>();
Serial.println("🔍 [DEBUG] ephemeral B64 payload:");
Serial.println(ephB64);

// turn it back into bytes
std::vector<uint8_t> ctEph;
if (!certModule::decodeBase64(ephB64, ctEph)) {
    Serial.println("❌ decodeBase64(ephemeral) failed!");
    return;
}
Serial.printf("🔍 [DEBUG] ctEph.size() = %u\n", (unsigned)ctEph.size());

// now decrypt
std::vector<uint8_t> derEph;
aesCtrDecryptRaw(ctEph, derEph);
Serial.printf("🔍 [DEBUG] decrypted eph blob len=%u first byte=0x%02X\n",
              (unsigned)derEph.size(),
              derEph.empty() ? 0 : derEph[0]);

if (derEph.empty()) {
    Serial.println("❌ derEph is empty – nothing to import!");
    return;
}

// if you’re working with raw X||Y 64-byte points, they need the 0x04 prefix:
if (derEph.size() == 64 && derEph[0] != 0x04) {
    Serial.println("⚠️ prefixing 0x04 to uncompressed point");
    derEph.insert(derEph.begin(), 0x04);
}

// import into your ECDH context
if (!ecdh.importPeerPublicKey(derEph)) {
    Serial.println("❌ importPeerPublicKey DER failed");
    return;
}
Serial.println("✅ imported commander ephemeral");

// 9) Derive shared secret
std::vector<uint8_t> shared;
if (!ecdh.deriveSharedSecret(shared)) {
    Serial.println("❌ deriveSharedSecret failed");
    return;
}
sharedSecret = std::move(shared);
Serial.println("✅ Shared secret OK");


    hasResponded   = true;
    // send our response
    sendResponse(instance->soldier->getCommandersInsertionOrder()[0]);
}

// ────────────────────────────────────────────────────────────────────────────
// 2) Reply back with our own DER-based handshake envelope
// ────────────────────────────────────────────────────────────────────────────
void SoldierECDHHandler::sendResponse(int toId) {
    Serial.println("📤 Preparing soldier response…");

    // ────────────────────────────────────────────────────────────
    // 2) EXTRACT commander’s public-key DER (not full cert)
    const auto& cert = soldier->getCommanders().at(toId).cert;
    constexpr size_t PUB_DER_BUF = 512;
    unsigned char derBuf[PUB_DER_BUF];
    int pubDerLen = mbedtls_pk_write_pubkey_der(
        const_cast<mbedtls_pk_context*>(&cert.pk),
        derBuf, PUB_DER_BUF
    );
    if (pubDerLen < 0) {
        Serial.printf("❌ commander pubkey DER extract failed: -0x%04X\n", -pubDerLen);
        return;
    }
    unsigned char* derStart = derBuf + (PUB_DER_BUF - pubDerLen);
    std::vector<uint8_t> pubDer(derStart, derStart + pubDerLen);

    // 3) Parse that DER into an mbedtls_pk_context
    mbedtls_pk_context cmdPub;
    mbedtls_pk_init(&cmdPub);
    int ret = mbedtls_pk_parse_public_key(
        &cmdPub,
        pubDer.data(),
        pubDer.size()
    );
    if (ret != 0) {
        char err[128];
        mbedtls_strerror(ret, err, sizeof(err));
        Serial.printf("❌ parse_public_key(DER) failed: -0x%04X (%s)\n", -ret, err);
        mbedtls_pk_free(&cmdPub);
        return;
    }
    Serial.println("✅ Commander RSA public key parsed (DER)");

    // ────────────────────────────────────────────────────────────
    // 4) Generate a 256-bit symmetric key + 128-bit IV
    uint8_t symKey[32], iv[16];
    mbedtls_ctr_drbg_random(&crypto.getDrbg(), symKey, sizeof(symKey));
    mbedtls_ctr_drbg_random(&crypto.getDrbg(), iv, sizeof(iv));

    // ────────────────────────────────────────────────────────────
    // 5) Wrap AES key with RSA-OAEP
    std::vector<uint8_t> wrappedKey(mbedtls_pk_get_len(&cmdPub));
    size_t wrappedLen = 0;
    if (mbedtls_pk_encrypt(&cmdPub,
                           symKey, sizeof(symKey),
                           wrappedKey.data(), &wrappedLen, wrappedKey.size(),
                           mbedtls_ctr_drbg_random, &crypto.getDrbg()) != 0)
    {
        Serial.println("❌ RSA wrap of symmetric key failed");
        mbedtls_pk_free(&cmdPub);
        return;
    }
    wrappedKey.resize(wrappedLen);

    // ────────────────────────────────────────────────────────────
    // 6) AES-CTR encrypt helper (captures symKey+iv)
    auto aesCtrEncryptRaw = [&](const std::vector<uint8_t>& plain,
                                std::vector<uint8_t>& out) {
        mbedtls_cipher_context_t ctx;
        mbedtls_cipher_init(&ctx);
        auto info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_CTR);
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

    // ────────────────────────────────────────────────────────────
    // 7) Encrypt our own DER cert + DER ECDH pubkey
    std::vector<uint8_t> derCert(
        soldier->getPublicCert().raw.p,
        soldier->getPublicCert().raw.p + soldier->getPublicCert().raw.len
    );
    std::vector<uint8_t> derEph = ecdh.getPublicKeyRaw();
    std::vector<uint8_t> ctCert, ctEph;
    aesCtrEncryptRaw(derCert, ctCert);
    aesCtrEncryptRaw(derEph,  ctEph);

    // ────────────────────────────────────────────────────────────
    // 8) Base64-encode everything
    String wrappedKeyB64 = certModule::toBase64(wrappedKey);
    String ivB64         = certModule::toBase64({ iv, iv + sizeof(iv) });
    String certCTB64     = certModule::toBase64(ctCert);
    String ephCTB64      = certModule::toBase64(ctEph);

    // Cleanup RSA context
    mbedtls_pk_free(&cmdPub);

    // ────────────────────────────────────────────────────────────
    // 9) Build JSON envelope and send
    DynamicJsonDocument doc(8192);
    doc["id"]        = toId;
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
        Serial.println("❌ Failed to send soldier response");
        return;
    }

    Serial.println("✅ Soldier response sent");

  // 2) install the secure‐msg handler
  awaitSecureMessage();
    return;
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

bool SoldierECDHHandler::hasReceivedSecureMessage() const {
    return hasReceiveMessage;
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

// void SoldierECDHHandler::poll() {
//   lora.readData();
//   //lora.cleanUpTransmissions();
// }




void SoldierECDHHandler::handleSecureLoRaDataStatic(const uint8_t* buf, size_t len) {
    Serial.printf("📥 handleSecureLoRaDataStatic called! len=%u\n", len);
    instance->securePkt.assign(buf, buf + len);
    instance->secureMsgPending = true;
}


void SoldierECDHHandler::handleSecureLoRaData(const uint8_t* data, size_t len) {
    Serial.printf("🔐 handleSecureLoRaData: %u bytes\n", len);
    DynamicJsonDocument doc(512);
    String in((const char*)data, len);
    if (deserializeJson(doc, in) != DeserializationError::Ok) {
        Serial.println("❌ secure JSON parse failed");
        return;
    }
    int rec = doc["id"].as<int>();
    Serial.printf("🔐 secure → parsed id %d (me=%u)\n",
                  rec, soldier->getSoldierNumber());
    if (rec != soldier->getSoldierNumber()) {
        Serial.println("⚠️ secure msg not for me");
        return;
    }

    std::vector<uint8_t> iv, ct;
    certModule::decodeBase64(doc["iv"].as<String>(), iv);
    certModule::decodeBase64(doc["msg"].as<String>(), ct);
    Serial.printf("🔐 secure → iv=%u bytes, ct=%u bytes\n",
                  iv.size(), ct.size());

    // 4) AES‐CTR decrypt with sharedSecret
    mbedtls_cipher_context_t ctx;
    mbedtls_cipher_init(&ctx);
    auto info = mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_256_CTR);
    mbedtls_cipher_setup(&ctx, info);
    mbedtls_cipher_setkey(&ctx, sharedSecret.data(), 256, MBEDTLS_DECRYPT);
    mbedtls_cipher_set_iv(&ctx, iv.data(), iv.size());
    mbedtls_cipher_reset(&ctx);

    size_t outl=0, finl=0;
    std::vector<uint8_t> pt(ct.size());
    mbedtls_cipher_update(&ctx, ct.data(), ct.size(), pt.data(), &outl);
    mbedtls_cipher_finish(&ctx, pt.data()+outl, &finl);
    mbedtls_cipher_free(&ctx);
    pt.resize(outl+finl);

    // 5) print
  String plain;
  for (auto c: pt) plain += char(c);
  Serial.print("📨 secure → “");
  Serial.print(plain);
  Serial.println("”");

  finalMessage = plain;
  hasReceiveMessage = true;
}

void SoldierECDHHandler::awaitSecureMessage() {
    Serial.println("⏳ awaitSecureMessage(): switching to RX for secure msg");

    // 1) switch radio back into RX
    lora.setup(/*transmissionMode=*/false);

    // 2) re-hook chunk-by-chunk callback
    lora.setOnReadData([](const uint8_t* pkt, size_t len) {
        Serial.printf("🔁 RX Chunk: len=%u\n", len);  // optional debug
        instance->lora.onLoraFileDataReceived(pkt, len);
    });

    // 3) hook the *file complete* callback (secure msg)
    Serial.println("🔁 Installing secure file handler");
    lora.setOnFileReceived(SoldierECDHHandler::handleSecureLoRaDataStatic);

    // 4) now it's safe to start RX
    lora.readData();

    // 5) start the polling timer
    fileReceiveTimer = lv_timer_create(
        [](lv_timer_t* t) {
            auto* self = static_cast<SoldierECDHHandler*>(t->user_data);
            self->lora.handleCompletedOperation();
            if (!self->lora.isBusy()) {
                Serial.println("🔁 poll() → rearming RX");
                self->lora.readData();
            }
        },
        100,
        this
    );
    
}




void SoldierECDHHandler::poll() {
  // 1) Drive LoRa ISR machinery
  lora.handleCompletedOperation();
  Serial.println("poll(): handleCompletedOperation done");

  // 2) Re-arm RX if idle
  if (!lora.isBusy()) {
    Serial.println("poll(): radio idle → rearming RX");
    lora.setup(false);
    lora.readData();
  }

  // ✅ 3) Check if a secure message is pending
  if (secureMsgPending) {
    Serial.println("poll(): secureMsgPending → calling handleSecureLoRaData");
    secureMsgPending = false;
    handleSecureLoRaData(securePkt.data(), securePkt.size());
  }
}

 String SoldierECDHHandler::getFinalMessage(){
    return finalMessage;
 }