#include "../src/handler/command_handler.hpp"
#include "../src/store/store.hpp"
#include <cassert>
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

    std::cout << "\n\u2713 All tests passed!\n";
    return 0;
}
