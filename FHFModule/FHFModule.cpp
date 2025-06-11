#include "FHFModule.h"


FHFModule::FHFModule(const std::vector<float>& freqs, uint16_t hopIntervalSeconds)
    : frequencies(freqs), hopInterval(hopIntervalSeconds)
{
    std::time_t now = std::time(nullptr);
    struct tm* timeinfo = std::localtime(&now);

    if (timeinfo) {
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", timeinfo);
        Serial.printf("[FHFModule] Time synced? Current time: %s\n", buf);
    } else {
        Serial.println("[FHFModule] Failed to get local time!");
    }

    Serial.printf("[FHFModule] Epoch now: %lld | EPOCH_START: %lld | Δ: %llds\n",
                  static_cast<int64_t>(now), static_cast<int64_t>(EPOCH_START),
                  static_cast<int64_t>(now) - static_cast<int64_t>(EPOCH_START));
}

int64_t FHFModule::currentHopSlot() const
{
    std::time_t now = std::time(nullptr);
    int64_t elapsed = static_cast<int64_t>(now) - EPOCH_START;
    return elapsed / hopInterval;
}

float FHFModule::currentFrequency() const
{
    int64_t slot = currentHopSlot();

    if (frequencies.empty()) {
        return 0;
    }

    return frequencies[slot % frequencies.size()];
}