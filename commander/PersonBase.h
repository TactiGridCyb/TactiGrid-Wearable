#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "mbedtls/x509_crt.h"


enum SoldiersStatus
{
    REGULAR,
    COMPROMISED,
    DEAD,
    SOS
};

struct CommanderInfo {
    std::string name;
    uint16_t commanderNumber;
    mbedtls_x509_crt cert;
    bool isComp;
    time_t lastTimeReceivedData;
    enum SoldiersStatus status;
};

struct SoldierInfo {
    std::string name;
    uint16_t soldierNumber;
    mbedtls_x509_crt cert;
    bool isComp;
    time_t lastTimeReceivedData;
    enum SoldiersStatus status;
};

template<typename InfoType>
class PersonBase {
public:
    void addOther(const InfoType& info) {
        uint16_t id = infoNumber(info);
        auto [it, inserted] = others.emplace(id, info);
        if (inserted) {
            insertionOrder.push_back(id);
        } else {
            it->second = info;
        }
    }

    void removeOther(uint8_t id) {
        others.erase(id);

        auto newEnd = std::remove(
            insertionOrder.begin(),
            insertionOrder.end(),
            id
        );

        insertionOrder.erase(newEnd, insertionOrder.end());
    }


    const std::unordered_map<uint16_t, InfoType>& getOthers() const {
        return others;
    }

    void updateReceivedData(uint8_t id)
    {
        static_assert(
            std::is_same<T,CommanderInfo>::value || std::is_same<T,SoldierInfo>::value,
                "PersonBase may only be instantiated with a class that has isComp var"
        );

        this->others.at(id).lastTimeReceivedData = millis();
    }

    void setCompromised(uint8_t id)
    {
        static_assert(
            std::is_same<T,CommanderInfo>::value || std::is_same<T,SoldierInfo>::value,
                "PersonBase may only be instantiated with a class that has isComp var"
        );

        this->others.at(id).isComp = true;
    }

    std::vector<std::pair<uint16_t, InfoType>> getOthersInInsertionOrder() const {
        std::vector<std::pair<uint16_t, InfoType>> orderedOthers;
        orderedOthers.reserve(insertionOrder.size());
        for (auto id : insertionOrder) {
            auto it = others.find(id);
            if (it != others.end()) {
                orderedOthers.emplace_back(*it);
            }
        }
        return orderedOthers;
    }
protected:
    std::unordered_map<uint16_t, InfoType> others;
    std::vector<uint16_t> insertionOrder;

    static float intervalMS;

    static uint16_t infoNumber(const CommanderInfo& info) { return info.commanderNumber; }
    static uint16_t infoNumber(const SoldierInfo& info) { return info.soldierNumber; }
};