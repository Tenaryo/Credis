#include "../src/geo/geo_score.hpp"
#include "../src/handler/command_handler.hpp"
#include "../src/protocol/resp_parser.hpp"
#include "../src/store/store.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <string>

static std::string make_geoadd_resp(std::string_view key,
                                    std::string_view lon,
                                    std::string_view lat,
                                    std::string_view member) {
    std::string input = "*5\r\n$6\r\nGEOADD\r\n";
    input += "$" + std::to_string(key.size()) + "\r\n" + std::string(key) + "\r\n";
    input += "$" + std::to_string(lon.size()) + "\r\n" + std::string(lon) + "\r\n";
    input += "$" + std::to_string(lat.size()) + "\r\n" + std::string(lat) + "\r\n";
    input += "$" + std::to_string(member.size()) + "\r\n" + std::string(member) + "\r\n";
    return input;
}

static bool is_resp_error(const std::string& resp) {
    return resp.starts_with("-") && resp.ends_with("\r\n");
}

static bool contains(std::string_view haystack, std::string_view needle) {
    return haystack.find(needle) != std::string_view::npos;
}

void test_geoadd_returns_integer_one() {
    Store store;
    CommandHandler handler(store);

    std::string input = "*5\r\n$6\r\nGEOADD\r\n$6\r\nplaces\r\n$10\r\n11.5030378\r\n$9\r\n48."
                        "164271\r\n$6\r\nMunich\r\n";
    auto response = handler.process(input);

    assert(response == ":1\r\n");

    std::cout << "\u2713 Test passed: GEOADD responds with :1\r\n";
}

void test_geoadd_invalid_longitude_returns_error() {
    Store store;
    CommandHandler handler(store);

    auto input = make_geoadd_resp("places", "181", "0.3", "test2");
    auto response = handler.process(input);

    assert(is_resp_error(response));
    assert(contains(response, "ERR"));
    assert(contains(response, "longitude"));

    std::cout << "\u2713 Test passed: GEOADD invalid longitude returns error\r\n";
}

void test_geoadd_invalid_latitude_returns_error() {
    Store store;
    CommandHandler handler(store);

    auto input = make_geoadd_resp("places", "10", "90", "test1");
    auto response = handler.process(input);

    assert(is_resp_error(response));
    assert(contains(response, "ERR"));
    assert(contains(response, "latitude"));

    std::cout << "\u2713 Test passed: GEOADD invalid latitude returns error\r\n";
}

void test_geoadd_both_invalid_returns_error_with_both_keywords() {
    Store store;
    CommandHandler handler(store);

    auto input = make_geoadd_resp("places", "200", "100", "test3");
    auto response = handler.process(input);

    assert(is_resp_error(response));
    assert(contains(response, "ERR"));
    assert(contains(response, "longitude"));
    assert(contains(response, "latitude"));

    std::cout << "\u2713 Test passed: GEOADD both invalid returns error with both keywords\r\n";
}

static std::string make_zrange_resp(std::string_view key, int64_t start, int64_t stop) {
    return "*4\r\n$6\r\nZRANGE\r\n$" + std::to_string(key.size()) + "\r\n" + std::string(key) +
           "\r\n$" + std::to_string(std::to_string(start).size()) + "\r\n" + std::to_string(start) +
           "\r\n$" + std::to_string(std::to_string(stop).size()) + "\r\n" + std::to_string(stop) +
           "\r\n";
}

void test_geoadd_stores_member_in_sorted_set() {
    Store store;
    CommandHandler handler(store);

    auto geoadd_resp = make_geoadd_resp("places", "2.2944692", "48.8584625", "Paris");
    auto add_response = handler.process(geoadd_resp);
    assert(add_response == ":1\r\n");

    auto zrange_resp = make_zrange_resp("places", 0, -1);
    auto range_response = handler.process(zrange_resp);
    assert(range_response == "*1\r\n$5\r\nParis\r\n");

    std::cout << "\u2713 Test passed: GEOADD stores member in sorted set, ZRANGE retrieves it\r\n";
}

void test_geoadd_duplicate_member_returns_zero() {
    Store store;
    CommandHandler handler(store);

    auto geoadd_resp = make_geoadd_resp("places", "2.2944692", "48.8584625", "Paris");
    auto first = handler.process(geoadd_resp);
    assert(first == ":1\r\n");

    auto second = handler.process(geoadd_resp);
    assert(second == ":0\r\n");

    auto zrange_resp = make_zrange_resp("places", 0, -1);
    auto range_response = handler.process(zrange_resp);
    assert(range_response == "*1\r\n$5\r\nParis\r\n");

    std::cout << "\u2713 Test passed: GEOADD duplicate member returns :0\r\n";
}

static std::string make_zscore_resp(std::string_view key, std::string_view member) {
    return "*3\r\n$6\r\nZSCORE\r\n$" + std::to_string(key.size()) + "\r\n" + std::string(key) +
           "\r\n$" + std::to_string(member.size()) + "\r\n" + std::string(member) + "\r\n";
}

static std::string extract_bulk_string(const std::string& resp) {
    auto first_crlf = resp.find("\r\n");
    if (first_crlf == std::string::npos)
        return "";
    auto data_start = first_crlf + 2;
    auto second_crlf = resp.find("\r\n", data_start);
    if (second_crlf == std::string::npos)
        return "";
    return resp.substr(data_start, second_crlf - data_start);
}

static void verify_geo_score(Store& store,
                             CommandHandler& handler,
                             std::string_view lon,
                             std::string_view lat,
                             std::string_view member,
                             std::string_view expected_score) {
    auto add_resp = handler.process(make_geoadd_resp("places", lon, lat, member));
    assert(add_resp == ":1\r\n");

    auto score_resp = handler.process(make_zscore_resp("places", member));
    auto score_str = extract_bulk_string(score_resp);
    assert(score_str == expected_score);
}

void test_geoadd_paris_score() {
    Store store;
    CommandHandler handler(store);

    verify_geo_score(store, handler, "2.2944692", "48.8584625", "Paris", "3663832614298053");

    std::cout << "\u2713 Test passed: GEOADD Paris score is correct\r\n";
}

void test_geoadd_multi_city_scores() {
    Store store;
    CommandHandler handler(store);

    verify_geo_score(store, handler, "-0.1278", "51.5074", "London", "2163557714755072");
    verify_geo_score(store, handler, "151.2093", "-33.8688", "Sydney", "3252046221964352");
    verify_geo_score(store, handler, "139.6917", "35.6895", "Tokyo", "4171231230197045");
    verify_geo_score(store, handler, "-74.006", "40.7128", "New York", "1791873974549446");

    std::cout << "\u2713 Test passed: GEOADD multi-city scores are correct\r\n";
}

void test_geoadd_boundary_scores() {
    Store store;
    CommandHandler handler(store);

    {
        auto resp = handler.process(make_geoadd_resp("bounds", "0", "85.05112878", "max_lat"));
        assert(resp == ":1\r\n");
        auto score_resp = handler.process(make_zscore_resp("bounds", "max_lat"));
        assert(!score_resp.starts_with("$-1"));
        assert(score_resp.starts_with("$"));
    }
    {
        auto resp = handler.process(make_geoadd_resp("bounds", "0", "-85.05112878", "min_lat"));
        assert(resp == ":1\r\n");
        auto score_resp = handler.process(make_zscore_resp("bounds", "min_lat"));
        assert(!score_resp.starts_with("$-1"));
        assert(score_resp.starts_with("$"));
    }
    {
        auto resp = handler.process(make_geoadd_resp("bounds", "180", "0", "max_lon"));
        assert(resp == ":1\r\n");
        auto score_resp = handler.process(make_zscore_resp("bounds", "max_lon"));
        assert(!score_resp.starts_with("$-1"));
        assert(score_resp.starts_with("$"));
    }
    {
        auto resp = handler.process(make_geoadd_resp("bounds", "-180", "0", "min_lon"));
        assert(resp == ":1\r\n");
        auto score_resp = handler.process(make_zscore_resp("bounds", "min_lon"));
        assert(!score_resp.starts_with("$-1"));
        assert(score_resp.starts_with("$"));
    }

    std::cout << "\u2713 Test passed: GEOADD boundary coordinates produce valid scores\r\n";
}

static std::string make_geopos_resp(std::string_view key, const std::vector<std::string>& members) {
    auto n = 2 + members.size();
    std::string input = "*" + std::to_string(n) + "\r\n$6\r\nGEOPOS\r\n";
    input += "$" + std::to_string(key.size()) + "\r\n" + std::string(key) + "\r\n";
    for (const auto& m : members) {
        input += "$" + std::to_string(m.size()) + "\r\n" + m + "\r\n";
    }
    return input;
}

static std::string
make_zadd_resp(std::string_view key, std::string_view score, std::string_view member) {
    return "*4\r\n$4\r\nZADD\r\n$" + std::to_string(key.size()) + "\r\n" + std::string(key) +
           "\r\n$" + std::to_string(score.size()) + "\r\n" + std::string(score) + "\r\n$" +
           std::to_string(member.size()) + "\r\n" + std::string(member) + "\r\n";
}

static std::vector<double> parse_geopos_coordinates(const std::string& resp) {
    std::vector<double> coords;
    size_t pos = 0;
    while ((pos = resp.find("$", pos)) != std::string::npos) {
        auto crlf = resp.find("\r\n", pos);
        if (crlf == std::string::npos)
            break;
        auto len_str = resp.substr(pos + 1, crlf - pos - 1);
        if (len_str == "-1" || len_str.empty())
            break;
        auto val_start = crlf + 2;
        auto val_end = resp.find("\r\n", val_start);
        if (val_end == std::string::npos)
            break;
        auto val_str = resp.substr(val_start, val_end - val_start);
        try {
            coords.push_back(std::stod(val_str));
        } catch (...) {
            break;
        }
        pos = val_end + 2;
    }
    return coords;
}

static void assert_coord_near(double actual, double expected, double tolerance) {
    auto diff = std::abs(actual - expected);
    assert(diff < tolerance);
}

void test_geopos_decodes_score_to_coordinates() {
    Store store;
    CommandHandler handler(store);

    handler.process(make_zadd_resp("location_key", "3663832614298053", "Foo"));

    auto resp = handler.process(make_geopos_resp("location_key", {"Foo"}));

    auto coords = parse_geopos_coordinates(resp);
    assert(coords.size() == 2);

    assert_coord_near(coords[0], 2.294472, 0.000001);
    assert_coord_near(coords[1], 48.858463, 0.000001);

    std::cout << "\u2713 Test passed: GEOPOS decodes score to correct lon/lat coordinates\r\n";
}

void test_geopos_existing_and_missing_member() {
    Store store;
    CommandHandler handler(store);

    handler.process(make_geoadd_resp("places", "-0.1278", "51.5074", "London"));

    auto resp = handler.process(make_geopos_resp("places", {"London", "missing"}));

    assert(resp.starts_with("*2\r\n"));
    assert(!resp.starts_with("*2\r\n*-1"));

    auto null_part = RespParser::encode_null_array();
    assert(resp.ends_with(null_part));

    std::cout << "\u2713 Test passed: GEOPOS returns position for existing and null for missing "
                 "member\r\n";
}

void test_geopos_nonexistent_key() {
    Store store;
    CommandHandler handler(store);

    auto resp = handler.process(make_geopos_resp("missing_key", {"London", "Munich"}));

    auto expected = "*2\r\n" + RespParser::encode_null_array() + RespParser::encode_null_array();
    assert(resp == expected);

    std::cout << "\u2713 Test passed: GEOPOS returns null arrays for non-existent key\r\n";
}

static std::string
make_geodist_resp(std::string_view key, std::string_view member1, std::string_view member2) {
    return "*4\r\n$7\r\nGEODIST\r\n$" + std::to_string(key.size()) + "\r\n" + std::string(key) +
           "\r\n$" + std::to_string(member1.size()) + "\r\n" + std::string(member1) + "\r\n$" +
           std::to_string(member2.size()) + "\r\n" + std::string(member2) + "\r\n";
}

static double parse_geodist_value(const std::string& resp) {
    auto bulk = extract_bulk_string(resp);
    assert(!bulk.empty());
    return std::stod(bulk);
}

void test_geodist_munich_paris() {
    Store store;
    CommandHandler handler(store);

    handler.process(make_geoadd_resp("places", "11.5030378", "48.164271", "Munich"));
    handler.process(make_geoadd_resp("places", "2.2944692", "48.8584625", "Paris"));

    auto resp = handler.process(make_geodist_resp("places", "Munich", "Paris"));
    auto dist = parse_geodist_value(resp);

    assert(std::abs(dist - 682477.7582) < 0.1);

    std::cout << "\u2713 Test passed: GEODIST Munich-Paris returns ~682477.7582 meters\r\n";
}

void test_geodist_missing_member() {
    Store store;
    CommandHandler handler(store);

    handler.process(make_geoadd_resp("places", "11.5030378", "48.164271", "Munich"));

    auto resp = handler.process(make_geodist_resp("places", "Munich", "Berlin"));
    assert(resp == RespParser::encode_null_bulk_string());

    std::cout << "\u2713 Test passed: GEODIST missing member returns null bulk string\r\n";
}

void test_geodist_nonexistent_key() {
    Store store;
    CommandHandler handler(store);

    auto resp = handler.process(make_geodist_resp("missing_key", "A", "B"));
    assert(resp == RespParser::encode_null_bulk_string());

    std::cout << "\u2713 Test passed: GEODIST non-existent key returns null bulk string\r\n";
}

static void assert_roundtrip(double orig_lat, double orig_lon, double tolerance) {
    auto score = geo::encode(orig_lat, orig_lon);
    auto coords = geo::decode(score);
    assert_coord_near(coords.lat, orig_lat, tolerance);
    assert_coord_near(coords.lon, orig_lon, tolerance);
}

void test_geo_decode_roundtrip() {
    assert_roundtrip(48.8584625, 2.2944692, 0.000003);
    assert_roundtrip(51.5074, -0.1278, 0.000003);
    assert_roundtrip(-33.8688, 151.2093, 0.000003);
    assert_roundtrip(35.6895, 139.6917, 0.000003);
    assert_roundtrip(40.7128, -74.006, 0.000003);

    std::cout << "\u2713 Test passed: geo::decode round-trip for multiple cities\r\n";
}

static std::string make_geosearch_resp(std::string_view key,
                                       std::string_view lon,
                                       std::string_view lat,
                                       std::string_view radius,
                                       std::string_view unit) {
    auto appender = [](std::string& out, std::string_view s) {
        out += "$" + std::to_string(s.size()) + "\r\n" + std::string(s) + "\r\n";
    };
    std::string input = "*8\r\n$9\r\nGEOSEARCH\r\n";
    appender(input, key);
    appender(input, "FROMLONLAT");
    appender(input, lon);
    appender(input, lat);
    appender(input, "BYRADIUS");
    appender(input, radius);
    appender(input, unit);
    return input;
}

static std::vector<std::string> parse_resp_bulk_strings(const std::string& resp) {
    std::vector<std::string> result;
    size_t pos = 0;
    if (resp.empty() || resp[0] != '*')
        return result;
    auto crlf = resp.find("\r\n");
    if (crlf == std::string::npos)
        return result;
    pos = crlf + 2;
    while (pos < resp.size()) {
        if (resp[pos] != '$')
            break;
        auto len_crlf = resp.find("\r\n", pos);
        if (len_crlf == std::string::npos)
            break;
        auto len_str = resp.substr(pos + 1, len_crlf - pos - 1);
        auto len = std::stoull(len_str);
        auto val_start = len_crlf + 2;
        auto val_end = resp.find("\r\n", val_start);
        if (val_end == std::string::npos)
            break;
        result.push_back(resp.substr(val_start, val_end - val_start));
        pos = val_end + 2;
    }
    return result;
}

static void assert_unordered_members(const std::vector<std::string>& actual,
                                     const std::vector<std::string>& expected) {
    assert(actual.size() == expected.size());
    for (const auto& e : expected) {
        assert(std::ranges::find(actual, e) != actual.end());
    }
}

void test_geosearch_basic_radius_search() {
    Store store;
    CommandHandler handler(store);

    handler.process(make_geoadd_resp("places", "11.5030378", "48.164271", "Munich"));
    handler.process(make_geoadd_resp("places", "2.2944692", "48.8584625", "Paris"));
    handler.process(make_geoadd_resp("places", "-0.0884948", "51.506479", "London"));

    auto resp1 = handler.process(make_geosearch_resp("places", "2", "48", "100000", "m"));
    auto members1 = parse_resp_bulk_strings(resp1);
    assert_unordered_members(members1, {"Paris"});

    auto resp2 = handler.process(make_geosearch_resp("places", "2", "48", "500000", "m"));
    auto members2 = parse_resp_bulk_strings(resp2);
    assert_unordered_members(members2, {"Paris", "London"});

    auto resp3 = handler.process(make_geosearch_resp("places", "11", "50", "300000", "m"));
    auto members3 = parse_resp_bulk_strings(resp3);
    assert_unordered_members(members3, {"Munich"});

    std::cout << "\u2713 Test passed: GEOSEARCH basic radius search\r\n";
}

void test_geosearch_nonexistent_key() {
    Store store;
    CommandHandler handler(store);

    auto resp = handler.process(make_geosearch_resp("nonexistent", "2", "48", "100", "m"));
    assert(resp == "*0\r\n");

    std::cout << "\u2713 Test passed: GEOSEARCH non-existent key returns empty array\r\n";
}

void test_geosearch_km_unit() {
    Store store;
    CommandHandler handler(store);

    handler.process(make_geoadd_resp("places", "11.5030378", "48.164271", "Munich"));

    auto resp = handler.process(make_geosearch_resp("places", "11", "50", "300", "km"));
    auto members = parse_resp_bulk_strings(resp);
    assert_unordered_members(members, {"Munich"});

    std::cout << "\u2713 Test passed: GEOSEARCH with km unit\r\n";
}

int main() {
    std::cout << "Running GEOADD command tests...\n\n";

    test_geoadd_returns_integer_one();
    test_geoadd_invalid_longitude_returns_error();
    test_geoadd_invalid_latitude_returns_error();
    test_geoadd_both_invalid_returns_error_with_both_keywords();
    test_geoadd_stores_member_in_sorted_set();
    test_geoadd_duplicate_member_returns_zero();
    test_geoadd_paris_score();
    test_geoadd_multi_city_scores();
    test_geoadd_boundary_scores();

    std::cout << "\nRunning GEOPOS command tests...\n\n";

    test_geopos_decodes_score_to_coordinates();
    test_geopos_existing_and_missing_member();
    test_geopos_nonexistent_key();

    std::cout << "\nRunning geo::decode unit tests...\n\n";

    test_geo_decode_roundtrip();

    std::cout << "\nRunning GEODIST command tests...\n\n";

    test_geodist_munich_paris();
    test_geodist_missing_member();
    test_geodist_nonexistent_key();

    std::cout << "\nRunning GEOSEARCH command tests...\n\n";

    test_geosearch_basic_radius_search();
    test_geosearch_nonexistent_key();
    test_geosearch_km_unit();

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
