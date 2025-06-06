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
    time_t lastTimeReceivedData;
    enum SoldiersStatus status;
};

struct SoldierInfo {
    std::string name;
    uint16_t soldierNumber;
    mbedtls_x509_crt cert;
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
        Serial.printf("addCommander for %d\n", id);
    }

    void addSoldier(const SoldierInfo& info) {
        uint16_t id = infoNumber(info);
        auto [it, inserted] = soldiers.emplace(id, info);

        Serial.printf("addSoldier for %d\n", id);
    }

    void removeFirstCommander() 
    {
        if (!this->commandersInsertionOrder.empty()) 
        {
            commandersInsertionOrder.erase(commandersInsertionOrder.begin());
        }
    }

    void removeCommander(uint8_t id)
    {
        this->commanders.erase(id);
    }

    void removeSoldier(uint8_t id)
    {
        this->soldiers.erase(id);
    }


    const std::unordered_map<uint8_t, CommanderInfo>& getCommanders() const {
        return commanders;
    }

    void updateReceivedData(uint8_t id)
    {
        if(this->soldiers.find(id) != this->soldiers.end())
        {
            this->soldiers.at(id).lastTimeReceivedData = millis();
            return;
        }
        else if(this->commanders.find(id) != this->commanders.end())
        {
            this->commanders.at(id).lastTimeReceivedData = millis();
        }
    }

    const std::vector<uint8_t>& getCommandersInsertionOrder() const 
    {
        return this->commandersInsertionOrder;
    }

    const std::unordered_map<uint8_t, SoldierInfo>& getSoldiers() const
    {
        return this->soldiers;
    }

public:
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