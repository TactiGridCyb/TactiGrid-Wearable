// CommanderECDHHandler.h
#pragma once
#include <ArduinoJson.h>
#include "../LoraModule/LoraModule.h"
#include "../commander-config/commander-config.h"
#include "../commander/Commander.h"
#include "../certModule/certModule.h"
#include "ECDHHelper.h"
#include <vector>

class CommanderECDHHandler {
public:
    CommanderECDHHandler(float freq, Commander* cmdr, certModule& crypt);
    void begin();
    bool startECDHExchange(int soldierId, const mbedtls_x509_crt& soldiersCert);
    std::vector<uint8_t> getSharedSecret();
    bool isExchangeComplete();
    LoraModule& getLoRa() { return lora; }
    void poll();
    bool sendSecureMessage(int soldierId, const String& plaintext);

    void sendGMKData();

    private:
    static void handleLoRaDataStatic(const uint8_t* data, size_t len);
    
    void handleLoRaData(const uint8_t* data, size_t len);


   static bool decodeBase64(const String& input, std::vector<uint8_t>& output);
   static String toBase64(const std::vector<uint8_t>& input);

   static CommanderECDHHandler* instance; // For static callback access

   //modify the code to match
   String getPeerPublicKeyPEM(int soldierId);
   String getCertificatePEM();

   LoraModule lora;
   Commander* commander;
   certModule& crypto;
   ECDHHelper ecdh;
   std::vector<uint8_t> sharedSecret;
   std::unordered_map<uint8_t, std::vector<uint8_t>> secretsList;
   bool waitingResponse;
   bool hasHandled;
   unsigned long startWait;
   const unsigned long TIMEOUT_MS = 15000;

   bool responsePending  = false;
   std::vector<uint8_t> pendingPacket;
};

/*
=== How This Works ===

This class wraps the entire ECDH process for the Commander in a reusable module:

1. `CommanderECDHHandler(float freq, CommanderConfigModule* cfg)`
   - Constructor initializes LoRa and references the config object.

2. `begin()`
   - Sets up LoRa and binds the receive handler.

3. `startECDHExchange(int soldierId)`
   - Generates an ephemeral key, Base64-encodes it and the commander's certificate,
     and sends a JSON payload over LoRa.
   - Waits for the soldier's response.

4. `handleLoRaData()`
   - Validates the soldier ID, decodes and verifies the certificate, decodes the
     ephemeral public key, and computes the shared secret.
   - The result is saved in `sharedSecret`.

5. `getSharedSecret()`
   - Returns the derived shared secret after successful handshake.
   - If exchange hasn't completed yet, returns an empty vector and prints a warning.

6. `isExchangeComplete()`
   - Returns true if the ECDH process has successfully completed.

This encapsulates the ECDH logic and message handling so you can call:

```cpp
CommanderECDHHandler ecdhHandler(868.0, &config);
ecdhHandler.begin();
if (ecdhHandler.startECDHExchange(1)) {
  if (ecdhHandler.isExchangeComplete()) {
    std::vector<uint8_t> secret = ecdhHandler.getSharedSecret();
  }
}
```
*/