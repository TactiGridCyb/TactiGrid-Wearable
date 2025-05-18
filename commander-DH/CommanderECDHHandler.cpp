// CommanderECDHHandler.cpp
#include "CommanderECDHHandler.h"
#include <mbedtls/base64.h>
#include <mbedtls/error.h>

CommanderECDHHandler* CommanderECDHHandler::instance = nullptr;

CommanderECDHHandler::CommanderECDHHandler(float freq, CommanderConfigModule* cfg)
    : lora(freq), config(cfg), waitingResponse(false), hasHandled(false) {
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
    if (!ecdh.generateEphemeralKey()) {
        Serial.println("❌ Failed to generate ephemeral key");
        return false;
    }

    String certB64 = config->getCertificatePEM();
    String ephB64 = toBase64(ecdh.getPublicKeyRaw());

    DynamicJsonDocument doc(4096);
    doc["id"] = soldierId;
    doc["cert"] = certB64;
    doc["ephemeral"] = ephB64;

    String out;
    serializeJson(doc, out);

    if (lora.sendFile((const uint8_t*)out.c_str(), out.length(), 180) != RADIOLIB_ERR_NONE) {
        Serial.println("❌ Failed to send init message");
        return false;
    }

    Serial.println("📤 Init message sent");
    waitingResponse = true;
    startWait = millis();
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

    String msg((const char*)data, len);
    Serial.println("📥 Received soldier response:");
    Serial.println(msg);

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, msg) != DeserializationError::Ok) {
        Serial.println("❌ JSON parse error");
        return;
    }

    int soldierId = doc["id"];
    if (std::find(instance->config->getSoldiers().begin(), instance->config->getSoldiers().end(), soldierId) == instance->config->getSoldiers().end()) {
        Serial.println("⚠️ Invalid soldier ID");
        return;
    }

    std::vector<uint8_t> certRaw;
    if (!decodeBase64(doc["cert"], certRaw)) {
        Serial.println("❌ Cert decode failed");
        return;
    }

    String certPem((char*)certRaw.data());
    if (!instance->crypto.loadSingleCertificate(certPem)) {
        Serial.println("❌ Cert parse failed");
        return;
    }

    if (!instance->crypto.verifyCertificate()) {
        Serial.println("❌ Cert verification failed");
        return;
    }

    std::vector<uint8_t> ephRaw;
    if (!decodeBase64(doc["ephemeral"], ephRaw)) {
        Serial.println("❌ Ephemeral decode failed");
        return;
    }

    if (!instance->ecdh.importPeerPublicKey(ephRaw)) {
        Serial.println("❌ Ephemeral key import failed");
        return;
    }

    if (!instance->ecdh.deriveSharedSecret(instance->sharedSecret)) {
        Serial.println("❌ Shared secret derivation failed");
        return;
    }

    Serial.println("✅ Shared secret OK");
    instance->hasHandled = true;
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

bool CommanderECDHHandler::decodeBase64(const String& input, std::vector<uint8_t>& output) {
    size_t inputLen = input.length();
    size_t decodedLen = (inputLen * 3) / 4 + 2;
    output.resize(decodedLen);
    size_t outLen = 0;
    int ret = mbedtls_base64_decode(output.data(), output.size(), &outLen, (const uint8_t*)input.c_str(), inputLen);
    if (ret != 0) {
        char errBuf[128];
        mbedtls_strerror(ret, errBuf, sizeof(errBuf));
        Serial.printf("❌ Base64 decode failed: -0x%04X (%s)\n", -ret, errBuf);
        return false;
    }
    output.resize(outLen);
    return true;
}