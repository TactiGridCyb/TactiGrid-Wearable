#include "LoraModule.h"

ICACHE_RAM_ATTR void LoraModule::setReceivedFlag()
{
    LoraModule::receivedFlag = true;
}

ICACHE_RAM_ATTR void LoraModule::setTransmittedFlag()
{
    LoraModule::transmittedFlag = true;
}

int16_t LoraModule::setup(bool transmissionMode)
{
    int16_t state = this->loraDevice.begin();

    if (state != RADIOLIB_ERR_NONE)
    {
        return state;
    }

    state = this->loraDevice.setFrequency(this->freq);

    if(transmissionMode)
    {
        this->loraDevice.setDio1Action(LoraModule::setTransmittedFlag);
    }
    else
    {
        this->loraDevice.setDio1Action(LoraModule::setReceivedFlag);
    }

    return state;
}

int16_t LoraModule::setupListening()
{
    int16_t state = this->loraDevice.startReceive();
    return state;
}


void LoraModule::setOnReadData(std::function<void(const String&)> callback)
{
    this->onReadData = callback;
}

int16_t LoraModule::readData()
{
    if(this->receivedFlag)
    {
        this->receivedFlag = false;

        String data;

        int16_t state = this->loraDevice.readData(data);

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
    if(this->transmittedFlag)
    {
        this->transmittedFlag = false;
        return this->loraDevice.finishTransmit();
    }

    return 1;
}

int16_t LoraModule::sendData(const char* data)
{
    return this->loraDevice.startTransmit(data);
}

LoraModule::LoraModule(float frequency)
{
    this->freq = frequency;
}

LoraModule::~LoraModule()
{
    this->loraDevice.finishTransmit();
}