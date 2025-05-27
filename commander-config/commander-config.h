// CommanderConfigModule.h
#pragma once
#include <ArduinoJson.h>
#include <LilyGoLib.h>
#include <string>
#include <vector>

class CommanderConfigModule {
public:
    // commanders constructor - takes all data from config file
    CommanderConfigModule(const String& rawJson);

    // geter's for each class variable
    String getGMK() const;
    int getId() const;
    std::vector<int> getHierarchy() const;
    std::vector<int> getSoldiers() const;
    String getPrivateKeyPEM() const;
    String getCertificatePEM() const;
    String getCaCertificatePEM() const;
    String getFHF() const;
    String getGKF() const;
    String getSoldierPublicKey(int id) const;

    //for testing only - implement functionality later
    String getPeerPublicKeyPEM() const;

private:
    String _gmk; // gmk
    int _id; // commanders id
    std::vector<int> _hierarchy; // commanders hierarchy
    std::vector<int> _soldiers; // id's of all the soldiers
    String _privateKeyPEM; // private key in pem
    //for testing only
    String _publicKeyPEMSoldier; // public key from soldier 1 in pem
    String _certificatePEM; // commanders certificate in pem
    String _caCertificatePEM; // ca's certificate as pem
    String _fhf;  // FHF as string
    String _gkf;  // GK function as string
    //each soldier public key
};
