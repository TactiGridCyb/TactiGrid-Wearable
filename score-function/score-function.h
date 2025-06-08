#pragma once
#include <Arduino.h>
#include <LilyGoLib.h>
#include <vector>

// Represents a single (latitude, longitude) point
struct LatLon {
    float lat;
    float lon;
};

class LeadershipEvaluator {
public:
    LeadershipEvaluator(float maxDistMeters = 1000.0f,
                        float distWeight = 0.5f,
                        float battWeight = 0.5f)
        : _maxDist(maxDistMeters), _wDist(distWeight), _wBatt(battWeight) {}

    // Calculate battery voltage in volts
    float getBatteryVolt() const {
        return watch.getBattVoltage();
    }

    // Estimate battery % from voltage (linear 3.2V → 4.2V)
    uint8_t getBatteryPct() const {
        float v = getBatteryVolt();
        float pct = (v - 3.2f) / (4.2f - 3.2f);
        return constrain((int)(pct * 100), 0, 100);
    }

    // Compute leadership score given soldier→commander pairs
    float calculateScore(const std::vector<std::pair<LatLon, LatLon>>& links) const {
        if (links.empty()) return 0.0f;

        float totalDist = 0.0f;
        for (const auto& link : links)
            totalDist += haversineDistance(link.first, link.second);

        float avgDist = totalDist / links.size();
        float normDist = min(avgDist, _maxDist) / _maxDist;
        float battScore = getBatteryPct() / 100.0f;

        return _wDist * (1.0f - normDist) + _wBatt * battScore;
    }

    // Print score and battery info to Serial
    void printScore(const std::vector<std::pair<LatLon, LatLon>>& links) const {
        float score = calculateScore(links);
        Serial.printf("Score: %.3f\nBatt: %d%% (%.2fV)\n",
                      score, getBatteryPct(), getBatteryVolt());
    }

private:
    // Compute haversine distance in meters
    float haversineDistance(const LatLon& a, const LatLon& b) const {
        const float R = 6371000.0f;
        float dLat = radians(b.lat - a.lat);
        float dLon = radians(b.lon - a.lon);
        float sa = sin(dLat / 2), sb = sin(dLon / 2);
        float h = sa * sa + cos(radians(a.lat)) * cos(radians(b.lat)) * sb * sb;
        return R * 2 * atan2(sqrt(h), sqrt(1.0f - h));
    }

    float _maxDist;
    float _wDist;
    float _wBatt;
};
