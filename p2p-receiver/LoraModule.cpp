#include "LoraModule.h"


ICACHE_RAM_ATTR void LoraModule::setFlag()
{
    LoraModule::receivedFlag = true;
}

int16_t LoraModule::setup()
{
    int16_t state = this->loraDevice.begin();

    if (state != RADIOLIB_ERR_NONE)
    {
        return state;
    }

    state = this->loraDevice.setFrequency(this->freq);

    if (state == RADIOLIB_ERR_INVALID_FREQUENCY)
    {
        return state;
    }

    this->loraDevice.setDio1Action(LoraModule::setFlag);
}



LoraModule::LoraModule(float frequency)
{
    this->freq = frequency;
}

LoraModule::~LoraModule()
{
    this->loraDevice.finishTransmit();
}