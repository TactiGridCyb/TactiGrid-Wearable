// CommanderConfigModule.cpp
#include "../commander-config/commander-config.h"  // wifi's existing module
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"


CommanderConfigModule::CommanderConfigModule(const String& rawJson) {
    DynamicJsonDocument doc(8192);  // adjust size if needed
    deserializeJson(doc, rawJson);

    _gmk = doc["GMK"].as<String>();
    _id = doc["id"].as<int>();

    for (JsonVariant v : doc["commander_hierarchy"].as<JsonArray>()) {
        _hierarchy.push_back(v.as<int>());
    }
    for (JsonVariant v : doc["soldiers"].as<JsonArray>()) {
        _soldiers.push_back(v.as<int>());
    }

    _privateKeyPEM = doc["private_key_pem"].as<String>();
    _certificatePEM = doc["certificate_pem"].as<String>();
    _caCertificatePEM = doc["ca_certificate_pem"].as<String>();
}

String CommanderConfigModule::getGMK() const {
    return _gmk;
}

int CommanderConfigModule::getId() const {
    return _id;
}

std::vector<int> CommanderConfigModule::getHierarchy() const {
    return _hierarchy;
}

std::vector<int> CommanderConfigModule::getSoldiers() const {
    return _soldiers;
}

String CommanderConfigModule::getPrivateKeyPEM() const {
    return _privateKeyPEM;
}

String CommanderConfigModule::getCertificatePEM() const {
    return _certificatePEM;
}

String CommanderConfigModule::getCaCertificatePEM() const {
    return _caCertificatePEM;
}
