#include <gtest/gtest.h>

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
