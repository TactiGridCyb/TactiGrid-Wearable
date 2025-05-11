// CommanderConfigModule.h
#pragma once
#include <ArduinoJson.h>
#include <LilyGoLib.h>
#include <string>
#include <vector>

class CommanderConfigModule {
public:
    CommanderConfigModule(const String& rawJson);

    String getGMK() const;
    int getId() const;
    std::vector<int> getHierarchy() const;
    std::vector<int> getSoldiers() const;
    String getPrivateKeyPEM() const;
    String getCertificatePEM() const;
    String getCaCertificatePEM() const;

private:
    String _gmk;
    int _id;
    std::vector<int> _hierarchy;
    std::vector<int> _soldiers;
    String _privateKeyPEM;
    String _certificatePEM;
    String _caCertificatePEM;
};

