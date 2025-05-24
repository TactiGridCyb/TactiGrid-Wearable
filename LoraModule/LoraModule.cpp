#include "LoraModule.h"
#define RADIOLIB_ERR_CHANNEL_BUSY -1000

volatile bool finishedFlag = false;

volatile bool activeJob = false;
volatile bool activeFileJob = false;

ICACHE_RAM_ATTR void setFinishedFlag(void)
{
    Serial.println("setFinishedFlag");

    finishedFlag = true;
    if(!activeFileJob)
    {
        activeJob = false;
    }
    
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

    if (this->loraDevice.setSpreadingFactor(8) == RADIOLIB_ERR_INVALID_SPREADING_FACTOR) {
        Serial.println(F("Selected spreading factor is invalid for this module!"));
        while (true);
    }

    if (this->loraDevice.setCodingRate(6) == RADIOLIB_ERR_INVALID_CODING_RATE) {
        Serial.println(F("Selected coding rate is invalid for this module!"));
        while (true);
    }

    if (this->loraDevice.setSyncWord(0xAB) != RADIOLIB_ERR_NONE) {
        Serial.println(F("Unable to set sync word!"));
        while (true);
    }

    if (this->loraDevice.setOutputPower(22) == RADIOLIB_ERR_INVALID_OUTPUT_POWER) {
        Serial.println(F("Selected output power is invalid for this module!"));
        while (true);
    }


    if (this->loraDevice.setCurrentLimit(140) == RADIOLIB_ERR_INVALID_CURRENT_LIMIT) {
        Serial.println(F("Selected current limit is invalid for this module!"));
        while (true);
    }

    if (this->loraDevice.setPreambleLength(12) == RADIOLIB_ERR_INVALID_PREAMBLE_LENGTH) {
        Serial.println(F("Selected preamble length is invalid for this module!"));
        while (true);
    }

    if (this->loraDevice.setCRC(true) == RADIOLIB_ERR_INVALID_CRC_CONFIGURATION) {
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

    this->transmissionMode = transmissionMode;
    this->loraDevice.setDio1Action(setFinishedFlag);
    
    return state;
}

void LoraModule::switchToTransmitterMode()
{
    Serial.println("switchToTransmitterMode");
    this->transmissionMode = true;
}

void LoraModule::switchToReceiverMode()
{
    Serial.println("switchToReceiverMode");
    this->transmissionMode = false;
}

int16_t LoraModule::setFrequency(float newFreq)
{
    if(activeJob || activeFileJob)
    {
        if(this->isSendData)
        {
            this->isSendData = false;
        }
        return this->freq;
    }

    Serial.printf("Changing freq to %.2f\n", newFreq);

    this->freq = newFreq;
    this->loraDevice.setFrequency(this->freq);
    this->isSendData = true;

    return this->freq;
}


int16_t LoraModule::setupListening()
{
    int16_t state = this->loraDevice.startReceive();
    return state;
}

bool LoraModule::isChannelFree()
{
    activeJob = true;
    int16_t result = this->loraDevice.scanChannel();

    return (result == RADIOLIB_CHANNEL_FREE);
}

// Use this function carefully, isChannelFree activates the transmitted flag.
bool LoraModule::canTransmit()
{   
    Serial.println("canTransmit");
    bool isFree = this->isChannelFree();
    delay(5);
    finishedFlag = false;
    
    return !finishedFlag && isFree;
}

void LoraModule::setOnReadData(std::function<void(const uint8_t* data, size_t len)> callback)
{
    this->onReadData = callback;
}

bool LoraModule::isBusy()
{
    return activeJob || activeFileJob;
}

int16_t LoraModule::readData()
{
    if(finishedFlag && !this->transmissionMode)
    {
        
        finishedFlag = false;
        uint8_t buf[512];
        size_t len = sizeof(buf);
        Serial.println("PRE READ");
        int16_t state = this->loraDevice.readData(buf, len);
        Serial.printf("POST READ %d\n", state);
        if (state != RADIOLIB_ERR_NONE) 
        {
            loraDevice.startReceive();
            return state;
        }
        size_t pktLen = this->loraDevice.getPacketLength();
        Serial.println("POST getPacketLength");
        this->onReadData(buf, pktLen);
        loraDevice.startReceive();
        Serial.println("POST startReceive");
    }

    return RADIOLIB_ERR_NONE;
}


int16_t LoraModule::sendData(const char* data, bool interrupt)
{
    Serial.println("this->finishedFlag: " + String(finishedFlag) + " " + String(this->isSendData));

    if(!this->isSendData)
    {
        return 0;
    }

    const int maxRetries = 10;
    int16_t status = RADIOLIB_ERR_CHANNEL_BUSY;

    for (int attempt = 0; attempt < maxRetries; ++attempt) {
        if (this->canTransmit()) {
            activeJob = true;
            Serial.println("Transmit");
            if(!interrupt)
            {
                status = this->loraDevice.transmit(data);
            }
            else
            {
                status = this->loraDevice.startTransmit(data);
            }
            Serial.println("Transmit status: " + String(status));
            break;
        } else {
            int retryDelayMs = random(50, 150);
            Serial.println("Channel busy, retrying in " + String(retryDelayMs) + " ms...");
            delay(retryDelayMs);
        }
    }

    return status;
}

static void dumpHex(const uint8_t* buf, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (buf[i] < 16) Serial.print('0');
        Serial.print(buf[i], HEX);
        Serial.print(' ');
    }
}

int16_t LoraModule::sendFile(const uint8_t* data,
                             size_t   length,
                             size_t   chunkSize)
{
    Serial.println("sendFile");
    Serial.println("***********************************");
    Serial.println((const char*)data);
    Serial.println("***********************************");
    if (!data || length == 0) 
    {
        return 0;
    }

    activeJob = true;
    activeFileJob = true;

    String initFrame  = String(kFileInitTag)  + ':' +
                        String(length)        + ':' +
                        String(chunkSize);

    Serial.println(initFrame);
    //int16_t state = this->sendData(initFrame.c_str());
    int16_t state = this->loraDevice.transmit(initFrame.c_str());
    delay(100);  // Give receiver time to process INIT

    if (state != RADIOLIB_ERR_NONE) 
    {
        return state;
    }

    uint16_t totalChunks = (length + chunkSize - 1) / chunkSize;
    for (uint16_t chunkIndex = 0; chunkIndex < totalChunks; ++chunkIndex)
    {
        delay(50);

        size_t offset = chunkIndex * chunkSize;
        size_t segLen = min(chunkSize, length - offset);

        uint8_t packet[4 + segLen];
        packet[0] = 0xAB;                    // sync byte for file-chunks
        packet[1] = (chunkIndex >> 8) & 0xFF;       // MSB of chunk number
        packet[2] = (chunkIndex & 0xFF);            // LSB
        packet[3] = static_cast<uint8_t>(segLen);
        memcpy(packet + 4, data + offset, segLen);

        Serial.print(F("chunk="));      Serial.print(chunkIndex);
        Serial.print(F("  offset="));   Serial.print(offset);
        Serial.print(F("  segLen="));   Serial.print(segLen);
        Serial.print(F("  packet: "));
        dumpHex(packet, 4 + segLen);
        state = this->loraDevice.transmit(packet, 4 + segLen); // Blocking call, waiting for it to finish before proceeding
        
    }


    delay(50);

    String endFrame = String(kFileEndTag);
    activeFileJob = false;

    return this->loraDevice.transmit(endFrame.c_str());
}

LoraModule::LoraModule(float frequency)
{
    this->freq = frequency;
}

LoraModule::~LoraModule()
{
    this->loraDevice.finishTransmit();
}


//Added for reciving lrage chunks of data
void LoraModule::onLoraFileDataReceived(const uint8_t* pkt, size_t len)
{
    if (len == 0) return;
    
    Serial.printf("📦 Incoming packet: %.*s\n", len, pkt);  // Print every received packet

    // === INIT Frame ===
    if (memcmp(pkt, kFileInitTag, strlen(kFileInitTag)) == 0) {
        size_t totalSize = 0;
        this->transferChunkSize = 0;

        activeJob = true;
        activeFileJob = true;

        // Parse the init line: must match commander format exactly
        if (sscanf((const char*)pkt, "FILE_INIT:%zu:%zu", &totalSize, &this->transferChunkSize) == 2) {
            this->expectedChunks = (totalSize + this->transferChunkSize - 1) / this->transferChunkSize;
            this->fileBuffer.clear();
            this->fileBuffer.resize(totalSize);
            this->receivedChunks = 0;

            Serial.printf("📥 INIT: totalSize=%zu, chunkSize=%zu, expectedChunks=%u\n",
                          totalSize, this->transferChunkSize, this->expectedChunks);
        } else {
            Serial.println("❌ INIT frame parse failed!");
        }
        return;
    }

    // === END Frame ===
    if (memcmp(pkt, kFileEndTag, strlen(kFileEndTag)) == 0) {
        if (this->onFileReceived && this->receivedChunks == this->expectedChunks) {
            Serial.printf("✅ File complete (%u chunks)\n", this->receivedChunks);
            this->onFileReceived(this->fileBuffer.data(), this->fileBuffer.size());
            Serial.printf("returning from function");
            return;
        } else {
            Serial.printf("❌ Incomplete file (%u/%u chunks)\n", this->receivedChunks, this->expectedChunks);
        }

        activeJob = false;
        activeFileJob = false;

        return;
    }

    // === Chunk Frame ===
    if (pkt[0] == 0xAB && len >= 4) {
        uint16_t chunkIndex = (pkt[1] << 8) | pkt[2];
        uint8_t chunkLen = pkt[3];
        size_t offset = chunkIndex * this->transferChunkSize;

        if (offset + chunkLen > this->fileBuffer.size()) {
            Serial.printf("❌ Chunk %u out of bounds (offset %zu + len %u > buffer %zu)\n",
                          chunkIndex, offset, chunkLen, this->fileBuffer.size());
            return;
        }

        memcpy(&this->fileBuffer[offset], pkt + 4, chunkLen);
        this->receivedChunks++;

        Serial.printf("🧩 Chunk #%u received: offset=%zu, len=%u, totalChunks=%u/%u\n",
                      chunkIndex, offset, chunkLen, this->receivedChunks, this->expectedChunks);
    }
}


void LoraModule::clearFinishedFlag()
{
    if(this->transmissionMode && finishedFlag)
    {
        Serial.println("clearFinishedFlag");
        finishedFlag = false;
    }
}