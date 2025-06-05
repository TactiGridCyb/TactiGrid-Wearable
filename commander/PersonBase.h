#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <LilyGoLib.h>
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


    const std::unordered_map<uint8_t, InfoType>& getOthers() const {
        return others;
    }

    const std::unordered_map<uint8_t, InfoType>& getComp() const {
        return comp;
    }

    void updateReceivedData(uint8_t id)
    {
        static_assert(
            std::is_same<InfoType,CommanderInfo>::value || std::is_same<InfoType,SoldierInfo>::value,
                "PersonBase may only be instantiated with a class that has isComp var"
        );

        this->others.at(id).lastTimeReceivedData = millis();
    }

    void setCompromised(uint8_t id)
    {
        static_assert(
            std::is_same<InfoType,CommanderInfo>::value || std::is_same<InfoType,SoldierInfo>::value,
                "PersonBase may only be instantiated with a class that has isComp var"
        );

        this->others.at(id).isComp = true;

        this->addComp(id);

    }

    const std::vector<uint8_t>& getOthersInsertionOrder() const 
    {
        return this->insertionOrder;
    }
protected:
    std::unordered_map<uint8_t, InfoType> others;
    std::unordered_map<uint8_t, InfoType> comp;
    std::vector<uint8_t> insertionOrder;


    void setOthers(const std::unordered_map<uint8_t, InfoType>& others)
    {
        this->others = others;
    }

    void setComp(const std::unordered_map<uint8_t, InfoType>& comp)
    {
        this->comp = comp;
    }

    void setInsertionOrders(const std::vector<uint8_t>& insertionOrder)
    {
        this->insertionOrder = insertionOrder;
    }

    static uint8_t infoNumber(const CommanderInfo& info) { return info.commanderNumber; }
    static uint8_t infoNumber(const SoldierInfo& info) { return info.soldierNumber; }

private:
    void addComp(const InfoType& info) {
        uint8_t id = infoNumber(info);
        auto [it, inserted] = comp.emplace(id, info);

        if (!inserted) 
        {
            it->second = info;
        }
    }
};