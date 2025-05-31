#include "LoraModule.h"
#include <CryptoModule.h>

std::unique_ptr<LoraModule> loraModule;


void init_p2p_test(const uint8_t* data, size_t len)
{
    String incoming;
    for (size_t i = 0; i < len; i++) {
        incoming += (char)data[i];
    }

    Serial.println("Receieved at sender " + incoming);
    
    loraModule->readData();
}

void setup()
{
    Serial.begin(115200);
    watch.begin(&Serial);
    loraModule = std::make_unique<LoraModule>(433.5);
    loraModule->setup(false);
    loraModule->setOnReadData(init_p2p_test);

    crypto::Key256 GMK = []() {
        crypto::Key256 key{};
        const char* raw = "0123456789abcdef0123456789abcdef"; 
        std::memcpy(key.data(), raw, 32);
        return key;
    }();

    std::string strGMK = crypto::CryptoModule::key256ToAsciiString(GMK);

    Serial.printf("STRING KEY: %s\n", strGMK.c_str());

}



void loop()
{
    delay(100);
}