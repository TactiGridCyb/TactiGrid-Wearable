#include "CryptoModule.h"
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace crypto {

    namespace {
        constexpr size_t KEY_LEN   = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;
        constexpr size_t NONCE_LEN = crypto_aead_xchacha20poly1305_ietf_NPUBBYTES;
        constexpr size_t TAG_LEN   = crypto_aead_xchacha20poly1305_ietf_ABYTES;

        void chk(int ok, const char* msg) {
            if (ok != 0) throw std::runtime_error(msg);
        }

    }

    void CryptoModule::init() {
        if (sodium_init() < 0) throw std::runtime_error("libsodium init failed");
    }

    Key256 CryptoModule::generateGMK() {
        Key256 key;
        randombytes_buf(key.data(), KEY_LEN);
        return key;
    }

    Key256 CryptoModule::deriveGK(const Key256& gmk,
                                uint64_t unixTime,
                                const std::string& missionInfo,
                                const ByteVec& salt,
                                uint32_t soldiers)
    {
        std::ostringstream oss;
        oss << unixTime << "|" << missionInfo << "|" << soldiers;
        std::string info = oss.str();

        unsigned char prk[crypto_kdf_hkdf_sha256_KEYBYTES];
        chk(crypto_kdf_hkdf_sha256_extract(prk,
            salt.data(), salt.size(),
            gmk.data(), gmk.size()), "hkdf extract");

        Key256 gk;
        chk(crypto_kdf_hkdf_sha256_expand(
            gk.data(), gk.size(),
            info.data(), info.size(),
            prk),
            "hkdf expand");

        sodium_memzero(prk, sizeof prk);
        return gk;
    }

    Ciphertext CryptoModule::encrypt(const Key256& gk, const ByteVec& pt) {
        Ciphertext ct;
        ct.nonce.resize(NONCE_LEN);
        randombytes_buf(ct.nonce.data(), NONCE_LEN);

        ct.data.resize(pt.size() + TAG_LEN);
        unsigned long long clen = 0;
        
        chk(crypto_aead_chacha20poly1305_ietf_encrypt(
            ct.data.data(), &clen,
            pt.data(), pt.size(),
            nullptr, 0,
            NULL, ct.nonce.data(), gk.data()), "encrypt");

        ct.tag.assign(ct.data.end() - TAG_LEN, ct.data.end());
        ct.data.resize(pt.size());
        return ct;
    }
    
    std::string CryptoModule::key256ToString(const Key256& key) {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0');
        for (unsigned char byte : key) {
            oss << std::setw(2) << static_cast<int>(byte);
        }
        return oss.str();
    }
    
    std::string CryptoModule::key256ToAsciiString(const Key256& key) {
        return std::string(reinterpret_cast<const char*>(key.data()), key.size());
    }

    ByteVec CryptoModule::decrypt(const Key256& gk, const Ciphertext& ct) {
        ByteVec full(ct.data);
        full.insert(full.end(), ct.tag.begin(), ct.tag.end());

        ByteVec pt(full.size() - TAG_LEN);
        unsigned long long ptlen = 0;

        if (crypto_aead_chacha20poly1305_ietf_decrypt(
            pt.data(), &ptlen,
            NULL,
            full.data(), full.size(),
            nullptr, 0,
            ct.nonce.data(), gk.data()) != 0)
        {
            throw std::runtime_error("decryption failed");
        }

        pt.resize(ptlen);
        return pt;
    }

    Key256 CryptoModule::strToKey256(const std::string& s) {
        if (s.size() != KEY_LEN) {
            throw std::runtime_error(
                "Expected string of length " + std::to_string(KEY_LEN)
                + ", got " + std::to_string(s.size())
            );
        }
        Key256 key = {};
        std::memcpy(key.data(), s.c_str(), KEY_LEN);
        return key;
    }

}
