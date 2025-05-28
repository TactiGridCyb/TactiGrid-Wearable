#pragma once
#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include <sodium.h>
#include <stdexcept>
#include <cstring>

namespace crypto {

using Key256 = std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_KEYBYTES>;
using ByteVec = std::vector<unsigned char>;

struct Ciphertext {
    ByteVec nonce;
    ByteVec data;
    ByteVec tag;
};


class CryptoModule {
public:
    static void init();
    static Key256 generateGMK();
    static Key256 deriveGK(const Key256& gmk,
                           uint64_t unixTime,
                           const std::string& missionInfo,
                           const ByteVec& salt,
                           uint32_t soldiers);

    static Ciphertext encrypt(const Key256& gk, const ByteVec& plaintext);
    static ByteVec decrypt(const Key256& gk, const Ciphertext& ct);

    static Key256 strToKey256(const std::string& s);

};

}
