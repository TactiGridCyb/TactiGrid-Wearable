#pragma once

#include <vector>
#include <cstdint>
#include <chrono>

class FHFModule {
public:
    FHFModule(const std::vector<uint32_t>& freqs, uint32_t hopIntervalSeconds);

    uint32_t currentFrequency() const;

private:
    std::vector<uint32_t> frequencies;
    uint32_t hopInterval;

    // Epoch start time in seconds since UNIX epoch (2025-05-21 00:00:00 UTC)
    static constexpr time_t EPOCH_START = 1753200000;

    int64_t currentHopSlot() const;
};