#pragma once

#include <string>
#include <map>
#include "mbedtls/x509_crt.h"

struct CommanderInfo {
    std::string name;
    uint16_t commanderNumber;
    mbedtls_x509_crt cert;
};

struct SoldierInfo {
    std::string name;
    uint16_t soldierNumber;
    mbedtls_x509_crt cert;
    
};

template<typename InfoType>
class PersonBase {
public:
    void addOther(const InfoType& info) {
        others[infoNumber(info)] = info;
    }
    void removeOther(uint16_t id) {
        others.erase(id);
    }
    const std::map<uint16_t, InfoType>& getOthers() const {
        return others;
    }
protected:
    std::map<uint16_t, InfoType> others;

    static uint16_t infoNumber(const CommanderInfo& info) { return info.commanderNumber; }
    static uint16_t infoNumber(const SoldierInfo& info) { return info.soldierNumber; }
};