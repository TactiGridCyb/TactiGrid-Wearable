#include "CryptoModule.h"
#include <iostream>
#include <iomanip>
#include <tuple>
#include <cstring>

using namespace crypto;

ByteVec serialize_floats(float lat, float lon);
std::tuple<float, float> deserialize_floats(const ByteVec& buffer);

struct GPSCoord {
    float lat1;
    float lon1;
    float lat2;
    float lon2;
};

int main() {
    CryptoModule::init();
    GPSCoord g = {2.1f, 3.2f, 4.8f, 4.5f};

    Key256 gmk = CryptoModule::generateGMK();
    ByteVec salt(16); randombytes_buf(salt.data(), salt.size());
    Key256 gk = CryptoModule::deriveGK(gmk, time(nullptr), "Mission-X", salt, 42);

    
    char* serializedStruct = reinterpret_cast<char*>(&g);
    ByteVec location_data(serializedStruct, serializedStruct + sizeof(GPSCoord));

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
        GPSCoord* newG = reinterpret_cast<GPSCoord*>(decrypted_bytes.data());

        std::cout << "NEW VALUE:" << " " << newG->lon1 << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Decryption failed: " << e.what() << "\n";
    }

    return 0;
}
