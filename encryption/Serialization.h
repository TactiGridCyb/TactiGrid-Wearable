#pragma once
#include <vector>
#include <tuple>
#include <cstring>

using ByteVec = std::vector<unsigned char>;

inline ByteVec serialize_floats(float lat, float lon) {
    ByteVec buffer(sizeof(float) * 2);
    std::memcpy(buffer.data(), &lat, sizeof(float));
    std::memcpy(buffer.data() + sizeof(float), &lon, sizeof(float));
    return buffer;
}

inline std::tuple<float, float> deserialize_floats(const ByteVec& buffer) {
    if (buffer.size() != sizeof(float) * 2) throw std::runtime_error("Invalid buffer size for float tuple");
    float lat, lon;
    std::memcpy(&lat, buffer.data(), sizeof(float));
    std::memcpy(&lon, buffer.data() + sizeof(float), sizeof(float));
    return std::make_tuple(lat, lon);
}
