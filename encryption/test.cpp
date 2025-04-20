#include "CryptoModule.h"
#include "Serialization.h"
#include <iostream>
#include <iomanip>
#include <sodium.h>
#include <tuple>
#include <cstring>

using namespace crypto;

ByteVec serialize_floats(float lat, float lon);
std::tuple<float, float> deserialize_floats(const ByteVec& buffer);

int main() {
    CryptoModule::init();

    Key256 gmk = CryptoModule::generateGMK();
    ByteVec salt(16); randombytes_buf(salt.data(), salt.size());
    Key256 gk = CryptoModule::deriveGK(gmk, time(nullptr), "Mission-X", salt, 42);

    float lat = 37.7749f;
    float lon = -122.4194f;

    ByteVec location_data = serialize_floats(lat, lon);
    Ciphertext encrypted_location = CryptoModule::encrypt(gk, location_data);

    auto printHex = [](const std::string& label, const ByteVec& data) {
        std::cout << label << ": ";
        for (unsigned char byte : data)
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
        std::cout << std::dec << std::endl;
    };

    printHex("Location Nonce", encrypted_location.nonce);
    printHex("Location Ciphertext", encrypted_location.data);
    printHex("Location Tag", encrypted_location.tag);

    try {
        ByteVec decrypted_bytes = CryptoModule::decrypt(gk, encrypted_location);
        auto [decoded_lat, decoded_lon] = deserialize_floats(decrypted_bytes);
        std::cout << "Decrypted Location: (" << decoded_lat << ", " << decoded_lon << ")\n";
    } catch (const std::exception& e) {
        std::cerr << "Decryption failed: " << e.what() << "\n";
    }

    return 0;
}
