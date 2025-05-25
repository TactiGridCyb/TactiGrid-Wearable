// SoldierConfigModule.h
#pragma once
#include <ArduinoJson.h>
#include <LilyGoLib.h>
#include <string>
#include <vector>

class SoldierConfigModule {
public:
    // soldiers constructor - takes all data from config file
    SoldierConfigModule(const String& rawJson);

    // geter's for each class variable
    String getGMK() const;
    int getId() const;
    std::vector<int> getHierarchy() const;
    String getPrivateKeyPEM() const;
    String getCertificatePEM() const;
    String getCaCertificatePEM() const;
    String getFHF() const;
    String getGKF() const;

private:
    String _gmk; // gmk
    int _id; // soldiers id
    std::vector<int> _hierarchy; // commanders hierarchy
    String _privateKeyPEM; // private key in pem
    String _certificatePEM; // soldiers certificate in pem
    String _caCertificatePEM; // ca's certificate as pem
    String _fhf;  // FHF as string
    String _gkf;  // GK function as string
};
