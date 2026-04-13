#pragma once

#include <cstdint>

namespace geo {

static constexpr double kLatMin = -85.05112878;
static constexpr double kLatMax = 85.05112878;
static constexpr double kLonMin = -180.0;
static constexpr double kLonMax = 180.0;
static constexpr double kLatRange = kLatMax - kLatMin;
static constexpr double kLonRange = kLonMax - kLonMin;
static constexpr uint32_t kGeoStep = 1u << 26;

inline auto spread(uint32_t v) -> uint64_t {
    auto x = static_cast<uint64_t>(v);
    x = (x | (x << 16)) & 0x0000FFFF0000FFFFULL;
    x = (x | (x << 8)) & 0x00FF00FF00FF00FFULL;
    x = (x | (x << 4)) & 0x0F0F0F0F0F0F0F0FULL;
    x = (x | (x << 2)) & 0x3333333333333333ULL;
    x = (x | (x << 1)) & 0x5555555555555555ULL;
    return x;
}

inline auto encode(double lat, double lon) -> uint64_t {
    auto norm_lat = static_cast<uint32_t>(kGeoStep * (lat - kLatMin) / kLatRange);
    auto norm_lon = static_cast<uint32_t>(kGeoStep * (lon - kLonMin) / kLonRange);
    return spread(norm_lat) | (spread(norm_lon) << 1);
}

} // namespace geo
