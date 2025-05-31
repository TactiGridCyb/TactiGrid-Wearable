#pragma once

#include <vector>
#include <cstdint>
#include <chrono>
#include <LilyGoLib.h>

#define HOP_INTERVAL_SEC 60

class FHFModule {
public:
    FHFModule(const std::vector<float>& freqs, uint16_t hopIntervalSeconds = HOP_INTERVAL_SEC);

    float currentFrequency() const;

private:
    std::vector<float> frequencies;
    uint16_t hopInterval;

    static constexpr time_t EPOCH_START = 1747785600;

    int64_t currentHopSlot() const;
};