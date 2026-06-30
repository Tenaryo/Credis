#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "aof/aof_manager.hpp"
#include "handler/command_handler.hpp"
#include "server/server_config.hpp"
#include "store/store.hpp"

using namespace credis::handler;
using namespace credis::aof;

class HandlerServerTest : public ::testing::Test {
  protected:
    credis::store::Store store_;
    credis::server::ServerConfig config_;
    CommandHandler handler_{store_, config_};
};

TEST_F(HandlerServerTest, InfoWithoutArgs) {
    auto response = handler_.process("*1\r\n$4\r\nINFO\r\n");
    EXPECT_TRUE(response.starts_with("$"));
    EXPECT_NE(response.find("role:master"), std::string::npos);
}

TEST_F(HandlerServerTest, InfoReplication) {
    auto response = handler_.process("*2\r\n$4\r\nINFO\r\n$11\r\nreplication\r\n");
    EXPECT_TRUE(response.starts_with("$"));
    EXPECT_NE(response.find("# Replication"), std::string::npos);
    EXPECT_NE(response.find("role:master"), std::string::npos);
}

TEST(ConfigGetAofOverrides, ReturnsFlagValues) {
    credis::store::Store store;
    credis::server::ServerConfig config;
    credis::aof::AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("mydir");
    aof.set_appendfilename("myfile.aof");
    aof.set_appendfsync("always");
    CommandHandler handler(store, config);
    handler.set_aof_manager(aof);

    auto r1 = handler.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$10\r\nappendonly\r\n");
    EXPECT_EQ(r1, "*2\r\n$10\r\nappendonly\r\n$3\r\nyes\r\n");

    auto r2 = handler.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$13\r\nappenddirname\r\n");
    EXPECT_EQ(r2, "*2\r\n$13\r\nappenddirname\r\n$5\r\nmydir\r\n");

    auto r3 = handler.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$14\r\nappendfilename\r\n");
    EXPECT_EQ(r3, "*2\r\n$14\r\nappendfilename\r\n$10\r\nmyfile.aof\r\n");

    auto r4 = handler.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$11\r\nappendfsync\r\n");
    EXPECT_EQ(r4, "*2\r\n$11\r\nappendfsync\r\n$6\r\nalways\r\n");
}

TEST(CommandHandlerAofIntegration, WriteCommandAppendsToAofFile) {
    auto tmpdir = std::filesystem::temp_directory_path() / "credis_test_aof_int_XXXXXX";
    auto dirname = tmpdir.string();
    if (::mkdtemp(dirname.data()) == nullptr) {
        FAIL() << "Failed to create temp directory";
    }
    std::string tmp_path = dirname;

    auto aof_dir = tmp_path + "/subdir";
    std::filesystem::create_directories(aof_dir);

    {
        std::ofstream mf(aof_dir + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(aof_dir + "/myapp.aof.1.incr.aof");
    }

    credis::store::Store store;
    credis::server::ServerConfig config;
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.open(tmp_path);

    CommandHandler handler(store, config);
    handler.set_aof_manager(aof);

    handler.process("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n");

    aof.close();

    std::ifstream in(aof_dir + "/myapp.aof.1.incr.aof");
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n");

    std::filesystem::remove_all(tmp_path);
}

TEST(CommandHandlerAofIntegration, MultipleWriteCommandsAppendInOrder) {
    auto tmpdir = std::filesystem::temp_directory_path() / "credis_test_aof_multi_XXXXXX";
    auto dirname = tmpdir.string();
    if (::mkdtemp(dirname.data()) == nullptr) {
        FAIL() << "Failed to create temp directory";
    }
    std::string tmp_path = dirname;

    auto aof_dir = tmp_path + "/subdir";
    std::filesystem::create_directories(aof_dir);

    {
        std::ofstream mf(aof_dir + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(aof_dir + "/myapp.aof.1.incr.aof");
    }

    credis::store::Store store;
    credis::server::ServerConfig config;
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.open(tmp_path);

    CommandHandler handler(store, config);
    handler.set_aof_manager(aof);

    handler.process("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n");
    handler.process("*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n200\r\n");

    aof.close();

    std::ifstream in(aof_dir + "/myapp.aof.1.incr.aof");
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto expected = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n"
                    "*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n200\r\n";
    EXPECT_EQ(content, expected);

    std::filesystem::remove_all(tmp_path);
}

TEST(AofReplayIntegration, ReplaysSetAndGetReturnsCorrectValue) {
    auto tmpdir = std::filesystem::temp_directory_path() / "credis_test_aof_replay_XXXXXX";
    auto dirname = tmpdir.string();
    if (::mkdtemp(dirname.data()) == nullptr) {
        FAIL() << "Failed to create temp directory";
    }
    std::string tmp_path = dirname;

    auto aof_dir = tmp_path + "/subdir";
    std::filesystem::create_directories(aof_dir);

    {
        std::ofstream mf(aof_dir + "/myapp.aof.manifest");
        mf << "file random.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(aof_dir + "/random.aof.1.incr.aof");
        af << "*3\r\n$3\r\nSET\r\n$4\r\nkey1\r\n$4\r\nval1\r\n";
    }

    credis::store::Store store;
    credis::server::ServerConfig config;
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");

    CommandHandler handler(store, config);

    auto content = aof.read_aof_content(tmp_path);
    ASSERT_FALSE(content.empty());
    handler.process(content);

    auto result = handler.process("*2\r\n$3\r\nGET\r\n$4\r\nkey1\r\n");
    EXPECT_EQ(result, "$4\r\nval1\r\n");

    std::filesystem::remove_all(tmp_path);
}

TEST_F(HandlerServerTest, ConfigGetDir) {
    config_.dir = "/tmp/mydir";
    CommandHandler handler(store_, config_);

    auto response = handler.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$3\r\ndir\r\n");
    EXPECT_EQ(response, "*2\r\n$3\r\ndir\r\n$10\r\n/tmp/mydir\r\n");
}

TEST_F(HandlerServerTest, ConfigGetDbfilename) {
    config_.dbfilename = "dump.rdb";
    CommandHandler handler(store_, config_);

    auto response = handler.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$10\r\ndbfilename\r\n");
    EXPECT_EQ(response, "*2\r\n$10\r\ndbfilename\r\n$8\r\ndump.rdb\r\n");
}

TEST_F(HandlerServerTest, ConfigGetDbfilenameEmpty) {
    auto response = handler_.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$10\r\ndbfilename\r\n");
    EXPECT_EQ(response, "*2\r\n$10\r\ndbfilename\r\n$-1\r\n");
}

TEST_F(HandlerServerTest, ConfigGetUnknownParam) {
    auto response = handler_.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$7\r\nunknown\r\n");
    EXPECT_EQ(response, "*0\r\n");
}

TEST_F(HandlerServerTest, ConfigGetAppendonlyWithoutAof) {
    auto response = handler_.process("*3\r\n$6\r\nCONFIG\r\n$3\r\nGET\r\n$10\r\nappendonly\r\n");
    EXPECT_EQ(response, "*2\r\n$10\r\nappendonly\r\n$0\r\n\r\n");
}
