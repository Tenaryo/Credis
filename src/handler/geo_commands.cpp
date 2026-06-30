#include "handler/geo_commands.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <utility>
#include <vector>

#include "geo/geo_score.hpp"
#include "protocol/resp_codec.hpp"
#include "store/store.hpp"
#include "util/parse.hpp"
#include "util/string_utils.hpp"

namespace credis::handler {

void handle_geoadd(CommandContext& ctx, const std::vector<std::string_view>& args) {
    if (!ctx.store.key_is_absent_or_holds<credis::store::SortedSet>(args[1])) {
        credis::protocol::encode_error_into(*ctx.out,
                                            "WRONGTYPE Operation against a key holding the wrong kind of value");
        return;
    }
    auto lon = credis::util::parse_double(args[2]);
    auto lat = credis::util::parse_double(args[3]);
    if (!lon || !lat) {
        credis::protocol::encode_error_into(*ctx.out, "ERR value is not a valid float");
        return;
    }

    bool lon_invalid = !std::isfinite(*lon) || *lon < credis::geo::kLonMin || *lon > credis::geo::kLonMax;
    bool lat_invalid = !std::isfinite(*lat) || *lat < credis::geo::kLatMin || *lat > credis::geo::kLatMax;

    if (lon_invalid || lat_invalid) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "ERR invalid longitude,latitude pair %.6f,%.6f", *lon, *lat);
        credis::protocol::encode_error_into(*ctx.out, buf);
        return;
    }

    auto score = static_cast<double>(credis::geo::encode(*lat, *lon));
    auto added = ctx.store.zadd(std::string(args[1]), score, std::string(args[4]));
    credis::protocol::encode_integer_into(*ctx.out, added);
}

void handle_geopos(CommandContext& ctx, const std::vector<std::string_view>& args) {
    const auto& key = args[1];
    auto count = args.size() - 2;

    *ctx.out += "*" + std::to_string(count) + "\r\n";

    for (size_t i = 2; i < args.size(); ++i) {
        auto score = ctx.store.zscore(key, args[i]);
        if (score) {
            auto coords = credis::geo::decode(static_cast<uint64_t>(*score));
            char lon_buf[32];
            char lat_buf[32];
            std::snprintf(lon_buf, sizeof(lon_buf), "%.17g", coords.lon);
            std::snprintf(lat_buf, sizeof(lat_buf), "%.17g", coords.lat);
            *ctx.out += "*2\r\n";
            credis::protocol::encode_bulk_string_into(*ctx.out, lon_buf);
            credis::protocol::encode_bulk_string_into(*ctx.out, lat_buf);
        } else {
            *ctx.out += "*-1\r\n";
        }
    }
}

void handle_geodist(CommandContext& ctx, const std::vector<std::string_view>& args) {
    auto score1 = ctx.store.zscore(args[1], args[2]);
    auto score2 = ctx.store.zscore(args[1], args[3]);
    if (!score1 || !score2) {
        credis::protocol::encode_null_bulk_string_into(*ctx.out);
        return;
    }
    auto c1 = credis::geo::decode(static_cast<uint64_t>(*score1));
    auto c2 = credis::geo::decode(static_cast<uint64_t>(*score2));
    auto dist = credis::geo::distance(c1.lat, c1.lon, c2.lat, c2.lon);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%.4f", dist);
    credis::protocol::encode_bulk_string_into(*ctx.out, buf);
}

void handle_geosearch(CommandContext& ctx, const std::vector<std::string_view>& args) {
    const auto& key = args[1];

    if (credis::util::to_upper(args[2]) != "FROMLONLAT") {
        credis::protocol::encode_error_into(*ctx.out, "ERR syntax error");
        return;
    }

    auto search_lon = credis::util::parse_double(args[3]);
    auto search_lat = credis::util::parse_double(args[4]);
    if (!search_lon || !search_lat) {
        credis::protocol::encode_error_into(*ctx.out, "ERR value is not a valid float");
        return;
    }

    if (credis::util::to_upper(args[5]) != "BYRADIUS") {
        credis::protocol::encode_error_into(*ctx.out, "ERR syntax error");
        return;
    }

    auto radius = credis::util::parse_double(args[6]);
    if (!radius) {
        credis::protocol::encode_error_into(*ctx.out, "ERR value is not a valid float");
        return;
    }

    auto unit = credis::util::to_upper(args[7]);
    static constexpr std::pair<std::string_view, double> kUnitFactors[]
        = {{"M", 1.0}, {"KM", 1000.0}, {"MI", 1609.34}, {"FT", 0.3048}};
    const auto* factor_it = std::ranges::find_if(kUnitFactors, [&](const auto& p) { return p.first == unit; });
    if (factor_it == std::end(kUnitFactors)) {
        credis::protocol::encode_error_into(*ctx.out, "ERR unsupported unit provided");
        return;
    }

    double radius_m = *radius * factor_it->second;

    auto all = ctx.store.zgetall(key);
    std::vector<std::string> matched;
    for (const auto& [member, score] : all) {
        auto coords = credis::geo::decode(static_cast<uint64_t>(score));
        auto dist = credis::geo::distance(*search_lat, *search_lon, coords.lat, coords.lon);
        if (dist <= radius_m) {
            matched.push_back(member);
        }
    }

    credis::protocol::encode_array_into(*ctx.out, matched);
}

} // namespace credis::handler
