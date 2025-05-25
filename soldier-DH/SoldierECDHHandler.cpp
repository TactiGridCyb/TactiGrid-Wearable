// SoldierECDHHandler.cpp
#include "SoldierECDHHandler.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include "mbedtls/base64.h"
#include <mbedtls/error.h>

SoldierECDHHandler* SoldierECDHHandler::instance = nullptr;

SoldierECDHHandler::SoldierECDHHandler(float freq, SoldierConfigModule* cfg, CryptoHelper& crypt)
    : lora(freq), config(cfg), crypto(crypt), hasResponded(false) {
    instance = this;
}

void SoldierECDHHandler::begin() {
    lora.setup(false); // receiver mode
    lora.setOnReadData([](const uint8_t* pkt, size_t len) {
        instance->lora.onLoraFileDataReceived(pkt, len);
    });
    lora.onFileReceived = handleLoRaData;
}

void SoldierECDHHandler::startListening() {
    hasResponded = false;
    lora.setupListening();
}

void SoldierECDHHandler::handleLoRaData(const uint8_t* data, size_t len) {
    if (instance->hasResponded) return;

    String msg((const char*)data, len);
    Serial.println("📥 Received commander message:");
    Serial.println(msg);

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, msg) != DeserializationError::Ok) {
        Serial.println("❌ JSON error");
        return;
    }

    int recipientId = doc["id"];
    if (recipientId != instance->config->getId()) {
        Serial.println("⚠️ Not for me");
        return;
    }

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
    Serial.println("✅ Commander cert valid");

    std::vector<uint8_t> ephRaw;
    if (!decodeBase64(doc["ephemeral"], ephRaw)) {
        Serial.println("❌ E key decode");
        return;
    }
    if (!instance->ecdh.importPeerPublicKey(ephRaw)) {
        Serial.println("❌ E key import");
        return;
    }
    if (!instance->ecdh.generateEphemeralKey()) {
        Serial.println("❌ My keygen");
        return;
    }
    if (!instance->ecdh.deriveSharedSecret(instance->sharedSecret)) {
        Serial.println("❌ DH derive");
        return;
    }
    Serial.println("✅ Shared secret OK");

    instance->hasResponded = true;
    sendResponse(doc["id"].as<int>());
}

void SoldierECDHHandler::sendResponse(int toId) {
    Serial.println("📤 Sending soldier response...");
    String cert = instance->config->getCertificatePEM();
    String ephB64 = toBase64(instance->ecdh.getPublicKeyRaw());

    DynamicJsonDocument doc(4096);
    doc["id"] = instance->config->getId(); // change to commander id 
    doc["cert"] = cert;
    doc["ephemeral"] = ephB64;

    String out;
    serializeJson(doc, out);
    if (instance->lora.sendFile((const uint8_t*)out.c_str(), out.length(), 180) != RADIOLIB_ERR_NONE) {
        Serial.println("❌ Failed to send back message to the commander");
    }
    Serial.println("✅ Response sent");
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
