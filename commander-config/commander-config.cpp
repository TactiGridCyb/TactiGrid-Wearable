// CommanderConfigModule.cpp
#include "commander-config.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"


CommanderConfigModule::CommanderConfigModule(const String& rawJson) {
    DynamicJsonDocument doc(16 * 1024);
    deserializeJson(doc, rawJson);

    _gmk = doc["GMK"] | "";
    _id = doc["id"] | -1;

    for (JsonVariant v : doc["commander_hierarchy"].as<JsonArray>()) {
        _hierarchy.push_back(v.as<int>());
    }

    // ✅ Only loop if "soldiers" key exists and is an array
    if (doc.containsKey("soldiers") && doc["soldiers"].is<JsonArray>()) {
        for (JsonVariant v : doc["soldiers"].as<JsonArray>()) {
            _soldiers.push_back(v.as<int>());
        }
    }

    _privateKeyPEM = doc["private_key_pem"] | "";
    _certificatePEM = doc["certificate_pem"] | "";
    _caCertificatePEM = doc["ca_certificate_pem"] | "";

    //TODO: add implementation to extract the soldiers public keys from the file
    //for testing only - implement true functionality late
    _publicKeyPEMSoldier = doc["1"] | "";

    _fhf = doc["FHF"] | "";
    _gkf = doc["GKF"] | "";
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

//for testing only - implement true functionality later
String CommanderConfigModule::getPeerPublicKeyPEM() const{
    return _publicKeyPEMSoldier;
}

String CommanderConfigModule::getCertificatePEM() const {
    return _certificatePEM;
}

String CommanderConfigModule::getCaCertificatePEM() const {
    return _caCertificatePEM;
}

String CommanderConfigModule::getSoldierPublicKey(int id) const{
    //TODO: implement later
    return _publicKeyPEMSoldier;
}
