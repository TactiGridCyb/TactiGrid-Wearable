#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <LilyGoLib.h>
#include <optional>
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
    std::optional<float> lat;
    std::optional<float> lon;
    enum SoldiersStatus status;
};

struct SoldierInfo {
    std::string name;
    uint16_t soldierNumber;
    mbedtls_x509_crt cert;
    time_t lastTimeReceivedData;
    std::optional<float> lat;
    std::optional<float> lon;
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

    void removeFirstCommanderFromInsertionOrder() 
    {
        if (!this->commandersInsertionOrder.empty()) 
        {
            commandersInsertionOrder.erase(commandersInsertionOrder.begin());
        }
    }

    void removeCommander(uint8_t id)
    {
        Serial.println("removeCommander");

        this->commanders.erase(id);

        auto it = std::find(this->commandersInsertionOrder.begin(), this->commandersInsertionOrder.end(), id);
        if(it != this->commandersInsertionOrder.end())
        {
            this->commandersInsertionOrder.erase(it);
        }

    }

    void removeSoldier(uint8_t id)
    {
        Serial.println("removeSoldier");
        this->soldiers.erase(id);
    }


    const std::unordered_map<uint8_t, CommanderInfo>& getCommanders() const {
        return commanders;
    }

    bool areCoordsValid(uint8_t id, bool isSoldier)
    {
        if(isSoldier)
        {
            return this->soldiers.at(id).lat && this->soldiers.at(id).lon;
        }

        return this->commanders.at(id).lat && this->commanders.at(id).lon;

    }

    std::pair<float,float> getLocation(uint8_t id, bool isSoldier)
    {
        if(isSoldier)
        {
            return {*this->soldiers.at(id).lat, *this->soldiers.at(id).lon};
        }


        return {*this->commanders.at(id).lat, *this->commanders.at(id).lon};
    }

    void updateReceivedData(uint8_t id, float lat, float lon)
    {
        if(this->soldiers.find(id) != this->soldiers.end())
        {
            this->soldiers.at(id).lastTimeReceivedData = millis();
            this->soldiers.at(id).lat = lat;
            this->soldiers.at(id).lon = lon;

            return;
        }
        else if(this->commanders.find(id) != this->commanders.end())
        {
            this->commanders.at(id).lastTimeReceivedData = millis();
            this->commanders.at(id).lat = lat;
            this->commanders.at(id).lon = lon;
        }
    }

    void resetAllData()
    {
        for(const auto& kv : this->commanders)
        {
            this->commanders.at(kv.first).lastTimeReceivedData = millis();
        }

        for(const auto& kv : this->soldiers)
        {
            this->soldiers.at(kv.first).lastTimeReceivedData = millis();
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

    void updateInsertionOrderByForbidden(const std::vector<uint8_t>& forb)
    {
        this->commandersInsertionOrder.erase(
            std::remove_if(this->commandersInsertionOrder.begin(), this->commandersInsertionOrder.end(), [&](uint8_t val) 
            {
                return std::find(forb.begin(), forb.end(), val) != forb.end();
            }),
            this->commandersInsertionOrder.end()
        );
    }

    void removeUntillReachedCommanderInsertion(uint8_t commID)
    {
        auto it = std::find(this->commandersInsertionOrder.begin(), this->commandersInsertionOrder.end(), commID);
        if (it != this->commandersInsertionOrder.end()) 
        {
            this->commandersInsertionOrder.erase(this->commandersInsertionOrder.begin(), it);
        }
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