//implement the .h functions
// CommanderConfigModule.cpp
#include "soldier-config.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"


SoldierConfigModule::SoldierConfigModule(const String& rawJson) {
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, rawJson);

    _gmk = doc["GMK"] | "";
    _id = doc["id"] | -1;

    for (JsonVariant v : doc["commander_hierarchy"].as<JsonArray>()) {
        _hierarchy.push_back(v.as<int>());
    }

    _privateKeyPEM = doc["private_key_pem"] | "";
    _certificatePEM = doc["certificate_pem"] | "";
    _caCertificatePEM = doc["ca_certificate_pem"] | "";

    //load the commanders public key as base64 pem
    _commanderPublicKeyPEM = doc["commander"] | "";

    _fhf = doc["FHF"] | "";
    _gkf = doc["GKF"] | "";
}


String SoldierConfigModule::getGMK() const {
    return _gmk;
}

int SoldierConfigModule::getId() const {
    return _id;
}

std::vector<int> SoldierConfigModule::getHierarchy() const {
    return _hierarchy;
}

String SoldierConfigModule::getPrivateKeyPEM() const {
    return _privateKeyPEM;
}

String SoldierConfigModule::getCertificatePEM() const {
    return _certificatePEM;
}

String SoldierConfigModule::getCaCertificatePEM() const {
    return _caCertificatePEM;
}

String SoldierConfigModule::getFHF() const {
    return _fhf;
}

String SoldierConfigModule::getGKF() const {
    return _gkf;
}

String SoldierConfigModule::getCommanderPubKey() {
    return _commanderPublicKeyPEM;
}
