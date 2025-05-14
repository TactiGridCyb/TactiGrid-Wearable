#include "LoraModule.h"

volatile bool receivedFlag = false;
volatile bool transmittedFlag = false;

ICACHE_RAM_ATTR void setReceivedFlag(void)
{
    receivedFlag = true;
}

ICACHE_RAM_ATTR void setTransmittedFlag(void)
{
    transmittedFlag = true;
}

int16_t LoraModule::setup(bool transmissionMode)
{
    int16_t state = this->loraDevice.begin();

    if (state != RADIOLIB_ERR_NONE)
    {
        return state;
    }

    state = this->loraDevice.setFrequency(this->freq);
    if (this->loraDevice.setBandwidth(125.0) == RADIOLIB_ERR_INVALID_BANDWIDTH) {
        Serial.println(F("Selected bandwidth is invalid for this module!"));
        while (true);
    }

    // set spreading factor to 10
    if (this->loraDevice.setSpreadingFactor(10) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
        Serial.println(F("Selected spreading factor is invalid for this module!"));
        while (true);
    }

    // set coding rate to 6
    if (this->loraDevice.setCodingRate(6) == RADIOLIB_ERR_INVALID_CODING_RATE) {
        Serial.println(F("Selected coding rate is invalid for this module!"));
        while (true);
    }

    // set LoRa sync word to 0xAB
    if (this->loraDevice.setSyncWord(0xAB) != RADIOLIB_ERR_NONE) {
        Serial.println(F("Unable to set sync word!"));
        while (true);
    }

    // set output power to 10 dBm (accepted range is -17 - 22 dBm)
    if (this->loraDevice.setOutputPower(22) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
        Serial.println(F("Selected output power is invalid for this module!"));
        while (true);
    }

    // set over current protection limit to 140 mA (accepted range is 45 - 240 mA)
    // NOTE: set value to 0 to disable overcurrent protection
    if (this->loraDevice.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
        Serial.println(F("Selected current limit is invalid for this module!"));
        while (true);
    }

    // set LoRa preamble length to 15 symbols (accepted range is 0 - 65535)
    if (this->loraDevice.setPreambleLength(15) == RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH) {
        Serial.println(F("Selected preamble length is invalid for this module!"));
        while (true);
    }

    // disable CRC
    if (this->loraDevice.setCRC(false) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION) {
        Serial.println(F("Selected CRC is invalid for this module!"));
        while (true);
    }

    if (this->loraDevice.setTCXO(3.0) == RADIOLIB_ERR_INVALID_TCXO_VOLTAGE) {
        Serial.println(F("Selected TCXO voltage is invalid for this module!"));
        while (true);
    }

    if (this->loraDevice.setDio2AsRfSwitch() != RADIOLIB_ERR_NONE) {
        Serial.println(F("Failed to set DIO2 as RF switch!"));
        while (true);
    }
    
    if(transmissionMode)
    {
        this->loraDevice.setDio1Action(setTransmittedFlag);
    }
    else
    {
        this->loraDevice.setDio1Action(setReceivedFlag);
    }

    return state;
}

int16_t LoraModule::setupListening()
{
    int16_t state = this->loraDevice.startReceive();
    return state;
}

bool LoraModule::isChannelFree()
{
    int16_t result = this->loraDevice.scanChannel();

    return (result == RADIOLIB_CHANNEL_FREE);
}

bool LoraModule::canTransmit()
{   
    bool isFree = this->isChannelFree();

    Serial.println("Checking if can transmit: " + String(isFree) + " " + String(this->initialTransmittion) + " " + String(transmittedFlag));
    return !transmittedFlag && isFree;
}

void LoraModule::setOnReadData(std::function<void(const String&)> callback)
{
    this->onReadData = callback;
}

int16_t LoraModule::readData()
{
    if(receivedFlag)
    {
        receivedFlag = false;
        Serial.println("Cleared received flag!");
        String data;

        int16_t state = this->loraDevice.readData(data);
        Serial.println("Cleared received flag! " + String(state) + " " + data);
        if(state != RADIOLIB_ERR_NONE)
        {
            return state;
        }

        this->onReadData(data);
        this->loraDevice.startReceive();
    }

    return RADIOLIB_ERR_NONE;
}

int16_t LoraModule::cleanUpTransmissions()
{
    if (transmittedFlag)
    {
        transmittedFlag = false;
        Serial.println("Cleaning this->transmittedFlag " + String(transmittedFlag));
        uint16_t status =  this->loraDevice.finishTransmit();
        Serial.println(String(status));
    }

    return RADIOLIB_ERR_NONE;
}

int16_t LoraModule::sendData(const char* data)
{
    Serial.println("this->transmittedFlag: " + String(transmittedFlag));
    uint16_t status = this->loraDevice.startTransmit(data);
    Serial.println(String(status));
    if (!this->initialTransmittion && status == RADIOLIB_ERR_NONE)
    {
        this->initialTransmittion = true;
    }

    return status;
}

int16_t LoraModule::sendFile(const uint8_t* data,
                             size_t   length,
                             size_t   chunkSize)
{
    if (!data || length == 0) 
    {
        return 0;
    }

    // -------- phase 1: announce transfer ---------------------------------
    String initFrame  = String(kFileInitTag)  + ':' +
                        String(length)        + ':' +
                        String(chunkSize);

    int16_t state = this->sendData(initFrame.c_str());
    if (state != RADIOLIB_ERR_NONE) 
    {
        return state;
    }

    // -------- phase 2: numbered chunks -----------------------------------
    uint16_t totalChunks = (length + chunkSize - 1) / chunkSize;
    for (uint16_t idx = 0; idx < totalChunks; ++idx)
    {
        // Wait until the previous packet has really left the air
        while (!this->canTransmit()) { delay(10); }

        size_t  offset  = idx * chunkSize;
        size_t  segLen  = min(chunkSize, length - offset);

        uint8_t packet[4 + segLen];
        packet[0] = 0xAB;                    // sync byte for file-chunks
        packet[1] = (idx >> 8) & 0xFF;       // MSB of chunk number
        packet[2] = (idx & 0xFF);            // LSB
        packet[3] = static_cast<uint8_t>(segLen);
        memcpy(packet + 4, data + offset, segLen);

        state = this->loraDevice.startTransmit(packet, 4 + segLen);
        if (state != RADIOLIB_ERR_NONE) { return state; }
    }

    String endFrame = String(kFileEndTag);
    return this->sendData(endFrame.c_str());
}

LoraModule::LoraModule(float frequency)
{
    this->freq = frequency;
}

LoraModule::~LoraModule()
{
    this->loraDevice.finishTransmit();
}