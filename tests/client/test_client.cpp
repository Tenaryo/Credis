#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <vector>

#include "aof/aof_manager.hpp"
#include "cli/cli_parser.hpp"

using namespace credis::cli;
using namespace credis::aof;
using credis::server::ReplicaConfig;

namespace {

std::vector<char*> make_argv(const std::vector<std::string>& args) {
    static std::vector<std::string> storage;
    storage = args;
    std::vector<char*> argv;
    for (auto& s : storage)
        argv.push_back(s.data());
    return argv;
}

} // namespace

TEST(ParsePortTest, ReturnsDefaultWhenNoPortArg) {
    auto argv = make_argv({"program"});
    EXPECT_EQ(parse_port(static_cast<int>(argv.size()), argv.data()), 6379);
}

TEST(ParsePortTest, ReturnsCustomValueWhenPortArgProvided) {
    auto argv = make_argv({"program", "--port", "9999"});
    EXPECT_EQ(parse_port(static_cast<int>(argv.size()), argv.data()), 9999);
}

TEST(ParseReplicaofTest, ReturnsNulloptWhenNoReplicaofArg) {
    auto argv = make_argv({"program"});
    EXPECT_EQ(parse_replicaof(static_cast<int>(argv.size()), argv.data()), std::nullopt);
}

TEST(ParseReplicaofTest, ReturnsHostAndPortForReplicaofArg) {
    auto argv = make_argv({"program", "--replicaof", "localhost 6380"});
    auto result = parse_replicaof(static_cast<int>(argv.size()), argv.data());
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->host, "localhost");
    EXPECT_EQ(result->port, 6380);
}

TEST(ParseDirTest, ReturnsEmptyStringWhenNoDirArg) {
    auto argv = make_argv({"program"});
    EXPECT_EQ(parse_dir(static_cast<int>(argv.size()), argv.data()), "");
}

TEST(ParseDirTest, ReturnsCustomPathForDirArg) {
    auto argv = make_argv({"program", "--dir", "/tmp/data"});
    EXPECT_EQ(parse_dir(static_cast<int>(argv.size()), argv.data()), "/tmp/data");
}

TEST(ParseDbfilenameTest, ReturnsEmptyStringWhenNoDbfilenameArg) {
    auto argv = make_argv({"program"});
    EXPECT_EQ(parse_dbfilename(static_cast<int>(argv.size()), argv.data()), "");
}

TEST(ParseDbfilenameTest, ReturnsCustomNameForDbfilenameArg) {
    auto argv = make_argv({"program", "--dbfilename", "dump.rdb"});
    EXPECT_EQ(parse_dbfilename(static_cast<int>(argv.size()), argv.data()), "dump.rdb");
}

TEST(ApplyAofOverridesTest, AllFlagsOverrideDefaults) {
    auto argv = make_argv({"program",
                           "--appendonly",
                           "yes",
                           "--appenddirname",
                           "mydir",
                           "--appendfilename",
                           "myfile.aof",
                           "--appendfsync",
                           "always"});
    AofManager aof;
    apply_aof_overrides(aof, static_cast<int>(argv.size()), argv.data());
    EXPECT_EQ(aof.appendonly(), "yes");
    EXPECT_EQ(aof.appenddirname(), "mydir");
    EXPECT_EQ(aof.appendfilename(), "myfile.aof");
    EXPECT_EQ(aof.appendfsync(), "always");
}
