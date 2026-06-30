#include <gtest/gtest.h>

#include <string>

#include "handler/command_handler.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

using namespace credis::handler;

class HandlerGeoTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
};

TEST_F(HandlerGeoTest, GeoaddPalermo) {
    auto response = handler_.process("*5\r\n$6\r\nGEOADD\r\n$3\r\nkey\r\n$9\r\n13.361389\r\n$9\r\n38.115556\r\n$7\r\n"
                                     "Palermo\r\n");
    EXPECT_EQ(response, ":1\r\n");
}

TEST_F(HandlerGeoTest, GeoposPalermo) {
    handler_.process("*5\r\n$6\r\nGEOADD\r\n$3\r\nkey\r\n$9\r\n13.361389\r\n$9\r\n38.115556\r\n$7\r\n"
                     "Palermo\r\n");
    auto response = handler_.process("*3\r\n$6\r\nGEOPOS\r\n$3\r\nkey\r\n$7\r\nPalermo\r\n");
    EXPECT_TRUE(response.starts_with("*1\r\n"));
    EXPECT_NE(response.find("13.361"), std::string::npos);
    EXPECT_NE(response.find("38.115"), std::string::npos);
}

TEST_F(HandlerGeoTest, GeoaddInvalidLongitude) {
    auto response = handler_.process("*5\r\n$6\r\nGEOADD\r\n$3\r\nkey\r\n$4\r\n-181\r\n$3\r\n0.0\r\n$4\r\ntest\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerGeoTest, GeoaddInvalidLatitude) {
    auto response = handler_.process("*5\r\n$6\r\nGEOADD\r\n$3\r\nkey\r\n$3\r\n0.0\r\n$3\r\n100\r\n$4\r\ntest\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerGeoTest, GeoaddWrongType) {
    handler_.process("*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nhello\r\n");
    auto response = handler_.process("*5\r\n$6\r\nGEOADD\r\n$3\r\nkey\r\n$3\r\n0.0\r\n$3\r\n0.0\r\n$4\r\ntest\r\n");
    EXPECT_TRUE(response.starts_with("-WRONGTYPE"));
}

TEST_F(HandlerGeoTest, GeoposMissingMember) {
    handler_.process("*5\r\n$6\r\nGEOADD\r\n$3\r\nkey\r\n$3\r\n0.0\r\n$3\r\n0.0\r\n$4\r\ntest\r\n");
    auto response = handler_.process("*3\r\n$6\r\nGEOPOS\r\n$3\r\nkey\r\n$4\r\nnull\r\n");
    EXPECT_EQ(response, "*1\r\n*-1\r\n");
}

TEST_F(HandlerGeoTest, GeodistBasic) {
    handler_.process("*5\r\n$6\r\nGEOADD\r\n$3\r\nkey\r\n$9\r\n13.361389\r\n$9\r\n38.115556\r\n$7\r\nPalermo\r\n");
    handler_.process("*5\r\n$6\r\nGEOADD\r\n$3\r\nkey\r\n$9\r\n15.087269\r\n$9\r\n37.502669\r\n$7\r\nCatania\r\n");
    auto response = handler_.process("*4\r\n$7\r\nGEODIST\r\n$3\r\nkey\r\n$7\r\nPalermo\r\n$7\r\nCatania\r\n");
    EXPECT_TRUE(response.starts_with("$"));
}

TEST_F(HandlerGeoTest, GeodistOneMissing) {
    handler_.process("*5\r\n$6\r\nGEOADD\r\n$3\r\nkey\r\n$9\r\n13.361389\r\n$9\r\n38.115556\r\n$7\r\nPalermo\r\n");
    auto response = handler_.process("*4\r\n$7\r\nGEODIST\r\n$3\r\nkey\r\n$7\r\nPalermo\r\n$7\r\nMissing\r\n");
    EXPECT_EQ(response, "$-1\r\n");
}

TEST_F(HandlerGeoTest, GeodistBothMissing) {
    auto response = handler_.process("*4\r\n$7\r\nGEODIST\r\n$5\r\nnokey\r\n$1\r\na\r\n$1\r\nb\r\n");
    EXPECT_EQ(response, "$-1\r\n");
}

TEST_F(HandlerGeoTest, GeosearchBasic) {
    handler_.process("*5\r\n$6\r\nGEOADD\r\n$3\r\nkey\r\n$9\r\n13.361389\r\n$9\r\n38.115556\r\n$7\r\nPalermo\r\n");
    handler_.process("*5\r\n$6\r\nGEOADD\r\n$3\r\nkey\r\n$9\r\n15.087269\r\n$9\r\n37.502669\r\n$7\r\nCatania\r\n");
    auto response = handler_.process(
        "*8\r\n$9\r\nGEOSEARCH\r\n$3\r\nkey\r\n$10\r\nFROMLONLAT\r\n$9\r\n15.087269\r\n$9\r\n37.502669\r\n$8\r\n"
        "BYRADIUS\r\n$6\r\n500000\r\n$1\r\nm\r\n");
    EXPECT_NE(response.find("Catania"), std::string::npos);
}

TEST_F(HandlerGeoTest, GeosearchInvalidSyntax) {
    auto response = handler_.process(
        "*8\r\n$9\r\nGEOSEARCH\r\n$3\r\nkey\r\n$10\r\nFROMMEMBER\r\n$3\r\n0.0\r\n$3\r\n0.0\r\n$8\r\nBYRADIUS\r\n"
        "$2\r\n10\r\n$1\r\nm\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerGeoTest, GeosearchInvalidRadius) {
    auto response = handler_.process(
        "*8\r\n$9\r\nGEOSEARCH\r\n$3\r\nkey\r\n$10\r\nFROMLONLAT\r\n$3\r\n0.0\r\n$3\r\n0.0\r\n$8\r\nBYRADIUS\r\n$3\r\n"
        "abc\r\n$1\r\nm\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}

TEST_F(HandlerGeoTest, GeosearchInvalidUnit) {
    handler_.process("*5\r\n$6\r\nGEOADD\r\n$3\r\nkey\r\n$3\r\n0.0\r\n$3\r\n0.0\r\n$4\r\ntest\r\n");
    auto response = handler_.process(
        "*8\r\n$9\r\nGEOSEARCH\r\n$3\r\nkey\r\n$10\r\nFROMLONLAT\r\n$3\r\n0.0\r\n$3\r\n0.0\r\n$8\r\nBYRADIUS\r\n$2\r\n"
        "10\r\n$2\r\nxx\r\n");
    EXPECT_TRUE(response.starts_with("-ERR"));
}
