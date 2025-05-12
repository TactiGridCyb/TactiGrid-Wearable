#pragma once
#include "mbedtls/ecdh.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include <vector>

class ECDHHelper {
public:
    ECDHHelper();
    ~ECDHHelper();

    bool generateEphemeralKey();                            // Generates ephemeral (pk/sk)
    std::vector<uint8_t> getPublicKeyRaw() const;           // Export public key in raw format
    bool importPeerPublicKey(const std::vector<uint8_t>&);  // Import other side's pubkey
    bool deriveSharedSecret(std::vector<uint8_t>&);         // Compute shared secret

private:
    mbedtls_ecdh_context ctx;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_entropy_context entropy;
    bool keyGenerated = false;
};
