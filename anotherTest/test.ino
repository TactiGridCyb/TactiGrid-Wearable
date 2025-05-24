#include "LoraModule.h"

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
    loraModule->setupListening();
    Serial.println("LORA IS READY1");
}



void loop()
{
    delay(20000);
    loraModule->sendData("SENDING DATA");
}