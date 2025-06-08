#include "CryptoModule.h"


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

    Key256 CryptoModule::deriveGK(
        const Key256& gmk,
        uint64_t       unixTime,
        const std::string& missionInfo,
        const ByteVec& salt,
        uint32_t       soldiers)
    {
        std::ostringstream oss;
        oss << unixTime << "|" << missionInfo << "|" << soldiers;
        std::string info = oss.str();

        unsigned char prk[crypto_auth_hmacsha256_BYTES];
        chk(
        crypto_auth_hmacsha256(
            prk,
            gmk.data(),
            (unsigned long long)gmk.size(),
            salt.data()
        ),
        "hkdf‐sha256 extract (HMAC)"
        );

        std::vector<unsigned char> buf;
        buf.reserve(info.size() + 1);
        buf.insert(buf.end(),
                reinterpret_cast<const unsigned char*>(info.data()),
                reinterpret_cast<const unsigned char*>(info.data()) + info.size());
        buf.push_back(0x01);

        Key256 gk;
        chk(
        crypto_auth_hmacsha256(
            gk.data(),
            buf.data(),
            (unsigned long long)buf.size(),
            prk
        ),
        "hkdf‐sha256 expand (HMAC)"
        );

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

    std::string CryptoModule::base64Encode(const uint8_t* data, size_t len) {
        size_t encodedLen = sodium_base64_encoded_len(len, sodium_base64_VARIANT_ORIGINAL);
        std::string encoded(encodedLen, '\0');
        sodium_bin2base64(&encoded[0], encodedLen, data, len, sodium_base64_VARIANT_ORIGINAL);

        encoded.erase(std::find(encoded.begin(), encoded.end(), '\0'), encoded.end());
        return encoded;
    }

    std::vector<uint8_t> CryptoModule::base64Decode(const std::string& s) {
        std::vector<uint8_t> decoded(s.size());
        size_t decodedLen = 0;
        if (sodium_base642bin(decoded.data(), decoded.size(), s.c_str(), s.size(), nullptr, &decodedLen, nullptr, sodium_base64_VARIANT_ORIGINAL) != 0) {
            throw std::runtime_error("Base64 decode failed");
        }
        decoded.resize(decodedLen);
        return decoded;
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

    Ciphertext CryptoModule::encryptFile(const Key256& gk, const std::string& filePath)
    {
        File f = FFat.open(filePath.c_str(), FILE_READ);
        if (!f) 
        {
            throw std::runtime_error("encryptFile: cannot open " + filePath);
        }

        size_t sz = f.size();
        ByteVec plaintext(sz);
        size_t read = f.read(plaintext.data(), sz);
        f.close();

        if (read != sz) 
        {
            throw std::runtime_error("encryptFile: failed reading full file");
        }

        Ciphertext ct = CryptoModule::encrypt(gk, plaintext);

        return ct;
    }

}