#pragma once
#include <map>
#include <string>
#include <CryptoModule.h>
#include <Soldier.h>

class Commander{
    private:
    uint16_t commandersID;
    std::string fullName;
    std::map<uint16_t, Soldier> soldiersMap;
    crypto::Key256 gmk;
    crypto::Key256 gk;
    
    public:
    Commander(uint16_t commandersID, std::string fullName, crypto::Key256 gmk);
    crypto::Key256 getGMK();
    crypto::Key256 getGK();
};