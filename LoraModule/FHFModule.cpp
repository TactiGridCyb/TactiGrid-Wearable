#include "FHFModule.h"
#include <ctime>

FHFModule::FHFModule(const std::vector<uint32_t>& freqs, uint32_t hopIntervalSeconds)
    : frequencies(freqs), hopInterval(hopIntervalSeconds)
{
}

int64_t FHFModule::currentHopSlot() const
{
    std::time_t now = std::time(nullptr);
    int64_t elapsed = static_cast<int64_t>(now) - EPOCH_START;
    return elapsed / hopInterval;
}

uint32_t FHFModule::currentFrequency() const
{
    int64_t slot = currentHopSlot();
    if (frequencies.empty()) {
        return 0;
    }
    return frequencies[slot % frequencies.size()];
}