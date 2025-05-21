#pragma once

#include <vector>
#include <cstdint>
#include <chrono>
#include <LilyGoLib.h>

class FHFModule {
public:
    FHFModule(const std::vector<float>& freqs, uint16_t hopIntervalSeconds);

    float currentFrequency() const;

private:
    std::vector<float> frequencies;
    uint16_t hopInterval;

    static constexpr time_t EPOCH_START = 1747785600;

    int64_t currentHopSlot() const;
};