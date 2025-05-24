#pragma once

#include <LilyGoLib.h>
#include <LV_Helper.h>

static constexpr const char* kFileInitTag  = "FILE_INIT";
static constexpr const char* kFileChunkTag = "FILE_CHUNK";
static constexpr const char* kFileEndTag   = "FILE_END";

class LoraModule
{
private:
    SX1262 loraDevice = newModule();

    bool isSendData = true;
    bool transmissionMode = true;

    std::function<void(const uint8_t* data, size_t len)> onReadData;

    std::vector<uint8_t> fileBuffer;
    uint16_t expectedChunks      = 0;
    uint16_t receivedChunks      = 0;
    size_t   transferChunkSize   = 0;

    float freq;

public:
    LoraModule(float);

    int16_t setup(bool);
    int16_t sendData(const char*, bool interrupt = false);
    int16_t readData();

    int16_t setupListening();

    bool isChannelFree();
    bool canTransmit();
    

    void setOnReadData(std::function<void(const uint8_t* data, size_t len)> callback);
    int16_t sendFile(const uint8_t* data,
                     size_t length,
                     size_t chunkSize = 200);

    int16_t setFrequency(float newFreq);

   void switchToTransmitterMode();
   void switchToReceiverMode();
   void clearFinishedFlag();

    // Called when a full file is received (optional)
    std::function<void(const uint8_t* data, size_t len)> onFileReceived;

    // File reassembly logic (call this from setOnReadData)
    void onLoraFileDataReceived(const uint8_t* pkt, size_t len);

    ~LoraModule();
};
