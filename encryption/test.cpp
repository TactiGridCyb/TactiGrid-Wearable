#include <sodium.h>
#include <iostream>
#include <iomanip>
#include <tuple>
#include <cstring>
#include <vector>

using ByteVec = std::vector<unsigned char>;
using Key256 = std::array<unsigned char, crypto_aead_xchacha20poly1305_ietf_KEYBYTES>;
constexpr size_t KEY_LEN = crypto_aead_xchacha20poly1305_ietf_KEYBYTES;

int main() {
    Key256 key;
    randombytes_buf(key.data(), KEY_LEN);

    auto printHex = [](const std::string& label, const ByteVec& data) {
        std::cout << label << ": ";
        for (unsigned char byte : data)
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
        std::cout << std::dec << std::endl;
    };
    
    printHex("GMK", ByteVec(key.begin(), key.end()));

    return 0;
}
