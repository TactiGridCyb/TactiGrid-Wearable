// SoldierECDHHandler.h
#pragma once
#include <ArduinoJson.h>
#include "../LoraModule/LoraModule.h"
#include "../certModule/certModule.h"
#include "../commander/Soldier.h"
#include "ECDHHelper.h"
#include <vector>

class SoldierECDHHandler {
public:
    SoldierECDHHandler(float freq, Soldier* soldier, certModule& crypto);
    void begin();
    void startListening();
    std::vector<uint8_t> getSharedSecret();
    bool hasRespondedToCommander() const;
    LoraModule& getLoRa() { return lora; }
    void poll();


private:
   static void handleLoRaDataStatic(const uint8_t* data, size_t len);

    void handleLoRaData(const uint8_t* data, size_t len);
    bool decodeBase64(const String& input, std::vector<uint8_t>& output);
    String toBase64(const std::vector<uint8_t>& input);
    void sendResponse(int toId);

    static SoldierECDHHandler* instance; // For static callback access

    //modify the code to match
    String getCommanderPubKey(int commanderId);
    String getCertificatePEM();

    LoraModule lora;
    Soldier* soldier;
    certModule& crypto;
    ECDHHelper ecdh;
    std::vector<uint8_t> sharedSecret;
    bool hasResponded;
};

/*
=== How This Works ===

This class encapsulates the soldier-side ECDH communication protocol:

1. `SoldierECDHHandler(float freq, CommanderConfigModule* cfg)`
   - Constructor initializes LoRa and uses the preloaded config object.

2. `begin()`
   - Sets up LoRa and binds the message receive handler.

3. `startListening()`
   - Prepares LoRa to receive the commander's message.

4. `handleLoRaData()`
   - Parses the JSON, verifies the commander's cert, imports the ephemeral key,
     generates the soldier's own ECDH keys, derives the shared secret, and responds.

5. `sendResponse()`
   - Sends a JSON object with the soldier's cert and ephemeral key.

6. `getSharedSecret()`
   - Returns the derived shared secret if the exchange was completed.

7. `hasRespondedToCommander()`
   - Returns true if soldier already replied to the commander.
*/