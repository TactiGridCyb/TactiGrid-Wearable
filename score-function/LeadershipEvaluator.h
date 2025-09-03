#pragma once
#include <Arduino.h>
#include <LilyGoLib.h>
#include <vector>

class LeadershipEvaluator {
public:
    LeadershipEvaluator(float maxDistMeters = 1000.0f,
                        float distWeight = 0.5f,
                        float battWeight = 0.5f)
        : _maxDist(maxDistMeters), _wDist(distWeight), _wBatt(battWeight) {}

    uint8_t getBatteryPct() const {
        return watch.getBatteryPercent();
    }

    float calculateScore(const std::vector<std::pair<float, float>>& links, const std::pair<float,float>& selfDist) const {
        if (links.empty()) return 0.0f;

        float totalDist = 0.0f;
        for (const auto& c : links) totalDist += haversineDistance(selfDist, c);

        float avgDist = totalDist / links.size();
        float normDist = min(avgDist, _maxDist) / _maxDist;
        float battScore = getBatteryPct() / 100.0f;

        return _wDist * (1.0f - normDist) + _wBatt * battScore;
    }

private:
    float haversineDistance(const std::pair<float, float>& a, const std::pair<float, float>& b) const
    {
        constexpr float R = 6371000.0f;

        const float lat1 = radians(a.first);
        const float lon1 = radians(a.second);
        const float lat2 = radians(b.first);
        const float lon2 = radians(b.second);

        const float dLat = lat2 - lat1;
        const float dLon = lon2 - lon1;
        
        const float sa = sinf(dLat * 0.5f);
        const float sb = sinf(dLon * 0.5f);

        float h = sa*sa + cosf(lat1) * cosf(lat2) * sb*sb;
        h = std::min(1.0f, std::max(0.0f, h));

        return 2.0f * R * atan2f(sqrtf(h), sqrtf(1.0f - h));
    }

    float _maxDist;
    float _wDist;
    float _wBatt;
};