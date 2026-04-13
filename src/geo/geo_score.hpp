#pragma once

#include <cmath>
#include <cstdint>
#include <numbers>

namespace geo {

static constexpr double kLatMin = -85.05112878;
static constexpr double kLatMax = 85.05112878;
static constexpr double kLonMin = -180.0;
static constexpr double kLonMax = 180.0;
static constexpr double kLatRange = kLatMax - kLatMin;
static constexpr double kLonRange = kLonMax - kLonMin;
static constexpr uint32_t kGeoStep = 1u << 26;
static constexpr double kEarthRadius = 6372797.560856;

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

inline auto unspread(uint64_t x) -> uint32_t {
    x = x & 0x5555555555555555ULL;
    x = (x | (x >> 1)) & 0x3333333333333333ULL;
    x = (x | (x >> 2)) & 0x0F0F0F0F0F0F0F0FULL;
    x = (x | (x >> 4)) & 0x00FF00FF00FF00FFULL;
    x = (x | (x >> 8)) & 0x0000FFFF0000FFFFULL;
    x = (x | (x >> 16)) & 0x00000000FFFFFFFFULL;
    return static_cast<uint32_t>(x);
}

struct Coordinates {
    double lat;
    double lon;
};

inline auto decode(uint64_t score) -> Coordinates {
    auto lat_hash = unspread(score);
    auto lon_hash = unspread(score >> 1);
    auto lat = (static_cast<double>(lat_hash) + 0.5) * kLatRange / kGeoStep + kLatMin;
    auto lon = (static_cast<double>(lon_hash) + 0.5) * kLonRange / kGeoStep + kLonMin;
    return {lat, lon};
}

inline auto distance(double lat1, double lon1, double lat2, double lon2) -> double {
    auto to_rad = [](double deg) { return deg * std::numbers::pi / 180.0; };
    auto dlat = to_rad(lat2 - lat1);
    auto dlon = to_rad(lon2 - lon1);
    auto a = std::sin(dlat / 2) * std::sin(dlat / 2) + std::cos(to_rad(lat1)) *
                                                           std::cos(to_rad(lat2)) *
                                                           std::sin(dlon / 2) * std::sin(dlon / 2);
    return kEarthRadius * 2 * std::asin(std::sqrt(a));
}

} // namespace geo
