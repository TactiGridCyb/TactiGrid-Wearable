#pragma once

#include <LilyGoLib.h>
#include <LV_Helper.h>

#include <atomic>
#include <functional>
#include <vector>
#include <cstdint>

#include <FHFModule.h>

static constexpr const char* kFileInitTag  = "FILE_INIT";
static constexpr const char* kFileChunkTag = "FILE_CHUNK";
static constexpr const char* kFileEndTag   = "FILE_END";

class LoraModule
{
public:
    enum class Op : uint8_t {
        None,
        Transmit,
        Receive,
        FileTx,
        FileRx,
        FreqSync
    };

    void setOnReadData(std::function<void(const uint8_t* data, size_t len)> cb) {
        onReadData = std::move(cb);
    }
    void setOnFileReceived(std::function<void(const uint8_t* data, size_t len)> cb) {
        onFileReceived = std::move(cb);
    }

    bool cancelReceive();

    explicit LoraModule(float frequency);
    ~LoraModule();

    int16_t setup(bool transmissionMode);
    int16_t sendData(const char* data, bool interrupt = false);
    int16_t readData();
    int16_t sendFile(const uint8_t* data, size_t length, size_t chunkSize = 200);
    float setFrequency(float newFreq);

    void switchToTransmitterMode();
    void switchToReceiverMode();

    bool isChannelFree();
    bool canTransmit();
    bool isBusy() const;

    void handleCompletedOperation();

    static void IRAM_ATTR dio1ISR();

    void onLoraFileDataReceived(const uint8_t* pkt, size_t len);

    void syncFrequency(const FHFModule* module);

    const std::function<void(const uint8_t* data, size_t len)>& getOnFileReceived();

    void resetD1O();

    void setupD1O();

    float getCurrentFreq() const;

private:
    bool tryStartOp(Op desired);

    void notifyOpFinished();

    void printCurrentOP();

    SX1262 loraDevice = newModule();

    float freq;
    bool transmissionMode = true;

    uint64_t lastFrequencyCheck;

    std::function<void(const uint8_t*, size_t)> onReadData;
    std::function<void(const uint8_t*, size_t)> onFileReceived;

    std::vector<uint8_t> fileBuffer;
    uint16_t expectedChunks = 0;
    uint16_t receivedChunks = 0;
    size_t transferChunkSize = 0;

    std::atomic<Op> currentOp { Op::None };
    std::atomic<bool> opFinished { false   };

    static LoraModule* instance;
};
