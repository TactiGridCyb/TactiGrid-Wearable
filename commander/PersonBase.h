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

class PersonBase {
public:
    void addCommander(const CommanderInfo& info) {
        uint16_t id = infoNumber(info);
        auto [it, inserted] = commanders.emplace(id, info);
        if (inserted) {
            commandersInsertionOrder.push_back(id);
        } else {
            it->second = info;
        }
    }

    void removeOther(uint8_t id) {
        commanders.erase(id);

        auto newEnd = std::remove(
            commandersInsertionOrder.begin(),
            commandersInsertionOrder.end(),
            id
        );

        commandersInsertionOrder.erase(newEnd, commandersInsertionOrder.end());
    }


    const std::unordered_map<uint8_t, CommanderInfo>& getCommanders() const {
        return commanders;
    }

    void updateReceivedData(uint8_t id)
    {
        this->commanders.at(id).lastTimeReceivedData = millis();
    }

    const std::vector<uint8_t>& getCommandersInsertionOrder() const 
    {
        return this->commandersInsertionOrder;
    }

    const std::unordered_map<uint8_t, SoldierInfo>& getSoldiers() const
    {
        return this->soldiers;
    }

protected:
    std::unordered_map<uint8_t, CommanderInfo> commanders;
    std::unordered_map<uint8_t, SoldierInfo> soldiers;
    std::vector<uint8_t> commandersInsertionOrder;

    void setSoldiers(const std::unordered_map<uint8_t, SoldierInfo>& soldiers)
    {
        this->soldiers = soldiers;
    }

    void setCommanders(const std::unordered_map<uint8_t, CommanderInfo>& commanders)
    {
        this->commanders = commanders;
    }

    void setInsertionOrders(const std::vector<uint8_t>& commandersInsertionOrder)
    {
        this->commandersInsertionOrder = commandersInsertionOrder;
    }

    static uint8_t infoNumber(const CommanderInfo& info) { return info.commanderNumber; }
    static uint8_t infoNumber(const SoldierInfo& info) { return info.soldierNumber; }

};