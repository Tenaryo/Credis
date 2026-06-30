#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "aof/aof_manager.hpp"
#include "store/store.hpp"

using namespace credis::aof;

namespace {

class AofManagerTest : public ::testing::Test {
  protected:
    void SetUp() override {
        tmpdir_ = std::filesystem::temp_directory_path() / "credis_test_aof_XXXXXX";
        auto dirname = tmpdir_.string();
        if (::mkdtemp(dirname.data()) == nullptr) {
            FAIL() << "Failed to create temp directory";
        }
        tmp_dir_path_ = dirname;
    }

    void TearDown() override {
        std::filesystem::remove_all(tmp_dir_path_);
    }

    std::string tmp_dir_path_;
    std::filesystem::path tmpdir_;
};

} // namespace

TEST_F(AofManagerTest, EnsureDirectoryCreatesDirWhenAppendonlyYes) {
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");

    auto expected = tmp_dir_path_ + "/subdir";
    EXPECT_FALSE(std::filesystem::exists(expected));

    aof.ensure_directory(tmp_dir_path_);

    EXPECT_TRUE(std::filesystem::exists(expected));
    EXPECT_TRUE(std::filesystem::is_directory(expected));
}

TEST_F(AofManagerTest, EnsureDirectoryDoesNothingWhenAppendonlyNo) {
    AofManager aof;
    aof.ensure_directory(tmp_dir_path_);

    auto expected = tmp_dir_path_ + "/" + aof.appenddirname();
    EXPECT_FALSE(std::filesystem::exists(expected));
}

TEST_F(AofManagerTest, EnsureDirectoryHandlesExistingDirectory) {
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");

    auto expected_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(expected_path);
    ASSERT_TRUE(std::filesystem::exists(expected_path));

    EXPECT_NO_THROW(aof.ensure_directory(tmp_dir_path_));
    EXPECT_TRUE(std::filesystem::exists(expected_path));
}

TEST_F(AofManagerTest, EnsureFileCreatesEmptyFileWhenAppendonlyYes) {
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");

    aof.ensure_directory(tmp_dir_path_);
    auto expected = tmp_dir_path_ + "/subdir/myapp.aof.1.incr.aof";
    EXPECT_FALSE(std::filesystem::exists(expected));

    aof.ensure_file(tmp_dir_path_);

    EXPECT_TRUE(std::filesystem::exists(expected));
    EXPECT_TRUE(std::filesystem::is_regular_file(expected));
    EXPECT_EQ(std::filesystem::file_size(expected), 0);
}

TEST_F(AofManagerTest, EnsureFileDoesNothingWhenAppendonlyNo) {
    AofManager aof;
    aof.ensure_file(tmp_dir_path_);

    auto expected = tmp_dir_path_ + "/" + aof.appenddirname() + "/" + aof.appendfilename() + ".1.incr.aof";
    EXPECT_FALSE(std::filesystem::exists(expected));
}

TEST_F(AofManagerTest, EnsureFileDoesNotOverwriteExistingFile) {
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");

    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    auto file_path = dir_path + "/myapp.aof.1.incr.aof";
    {
        std::ofstream f(file_path);
        f << "old data";
    }
    ASSERT_GT(std::filesystem::file_size(file_path), 0);

    aof.ensure_file(tmp_dir_path_);

    EXPECT_TRUE(std::filesystem::exists(file_path));
    EXPECT_GT(std::filesystem::file_size(file_path), 0);
}

TEST_F(AofManagerTest, EnsureManifestCreatesFileWithCorrectContent) {
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");

    aof.ensure_directory(tmp_dir_path_);
    auto expected = tmp_dir_path_ + "/subdir/myapp.aof.manifest";
    EXPECT_FALSE(std::filesystem::exists(expected));

    aof.ensure_manifest(tmp_dir_path_);

    EXPECT_TRUE(std::filesystem::exists(expected));
    EXPECT_TRUE(std::filesystem::is_regular_file(expected));

    std::ifstream in(expected);
    std::string content;
    std::getline(in, content);
    EXPECT_EQ(content, "file myapp.aof.1.incr.aof seq 1 type i");
}

TEST_F(AofManagerTest, EnsureManifestDoesNothingWhenAppendonlyNo) {
    AofManager aof;
    aof.ensure_manifest(tmp_dir_path_);

    auto expected = tmp_dir_path_ + "/" + aof.appenddirname() + "/" + aof.appendfilename() + ".manifest";
    EXPECT_FALSE(std::filesystem::exists(expected));
}

TEST_F(AofManagerTest, OpenReadsManifestAndAppendsToCorrectFile) {
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");

    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);

    auto custom_aof = "custom.aof.1.incr.aof";
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file " << custom_aof << " seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/" + custom_aof);
    }

    aof.open(tmp_dir_path_);
    aof.append("test_data");
    aof.close();

    std::ifstream in(dir_path + "/" + custom_aof);
    std::string content;
    std::getline(in, content);
    EXPECT_EQ(content, "test_data");
}

TEST_F(AofManagerTest, AppendWritesRespCommand) {
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");

    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);

    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    aof.open(tmp_dir_path_);

    const auto* resp = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n";
    aof.append(resp);
    aof.close();

    std::ifstream in(dir_path + "/myapp.aof.1.incr.aof");
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(content, resp);
}

TEST_F(AofManagerTest, AppendWithAlwaysFsyncDoesNotCrash) {
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appendfsync("always");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");

    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    aof.open(tmp_dir_path_);
    aof.append("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n");
    aof.close();

    EXPECT_TRUE(std::filesystem::file_size(dir_path + "/myapp.aof.1.incr.aof") > 0);
}

TEST_F(AofManagerTest, ReadAofContentFromManifestSpecifiedFile) {
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");

    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);

    const auto* resp = "*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n";
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file random.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/random.aof.1.incr.aof");
        af << resp;
    }

    auto content = aof.read_aof_content(tmp_dir_path_);
    EXPECT_EQ(content, resp);
}

TEST_F(AofManagerTest, EverysecThreadStartsOnOpen) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.set_appendfsync("everysec");
    aof.open(tmp_dir_path_);

    EXPECT_TRUE(aof.is_fsync_thread_running());

    aof.close();
    EXPECT_FALSE(aof.is_fsync_thread_running());
}

TEST_F(AofManagerTest, NoThreadWhenFsyncNotEverysec) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appendfilename("myapp.aof");
    aof.set_appendfsync("always");
    aof.open(tmp_dir_path_);

    EXPECT_FALSE(aof.is_fsync_thread_running());

    aof.close();
}

TEST_F(AofManagerTest, EverysecAppendSurvivesThreadFsync) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appendfilename("myapp.aof");
    aof.set_appendfsync("everysec");
    aof.open(tmp_dir_path_);

    aof.append("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n");
    aof.append("*3\r\n$3\r\nSET\r\n$3\r\nbar\r\n$3\r\n200\r\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    aof.close();
}

TEST_F(AofManagerTest, SwitchEverysecToAlwaysStopsThread) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.set_appendfsync("everysec");
    aof.open(tmp_dir_path_);
    EXPECT_TRUE(aof.is_fsync_thread_running());

    aof.set_appendfsync("always");
    EXPECT_EQ(aof.appendfsync(), "always");
    EXPECT_FALSE(aof.is_fsync_thread_running());

    aof.close();
}

TEST_F(AofManagerTest, SwitchNoToEverysecStartsThread) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.set_appendfsync("no");
    aof.open(tmp_dir_path_);
    EXPECT_FALSE(aof.is_fsync_thread_running());

    aof.set_appendfsync("everysec");
    EXPECT_EQ(aof.appendfsync(), "everysec");
    EXPECT_TRUE(aof.is_fsync_thread_running());

    aof.close();
}

TEST_F(AofManagerTest, SetAppendfsyncSameValueNoOp) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.set_appendfsync("everysec");
    aof.open(tmp_dir_path_);
    EXPECT_TRUE(aof.is_fsync_thread_running());

    aof.set_appendfsync("everysec");
    EXPECT_TRUE(aof.is_fsync_thread_running());

    aof.close();
}

TEST_F(AofManagerTest, StartRewriteWhenAppendonlyOffReturnsFalse) {
    credis::store::Store store;
    AofManager aof;
    EXPECT_FALSE(aof.start_rewrite(store, tmp_dir_path_));
}

TEST_F(AofManagerTest, RewriteEmptyStoreCreatesFile) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    credis::store::Store store;
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.open(tmp_dir_path_);

    ASSERT_TRUE(aof.start_rewrite(store, tmp_dir_path_));
    EXPECT_TRUE(aof.is_rewriting());

    while (!aof.check_rewrite_complete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    EXPECT_FALSE(aof.is_rewriting());

    aof.close();
}

TEST_F(AofManagerTest, RewriteStringKeysAndReplay) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    credis::store::Store store;
    store.set("key1", "val1");
    store.set("key2", "val2");

    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.open(tmp_dir_path_);

    ASSERT_TRUE(aof.start_rewrite(store, tmp_dir_path_));

    while (!aof.check_rewrite_complete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    aof.close();

    auto content = aof.read_aof_content(tmp_dir_path_);
    EXPECT_NE(content.find("SET"), std::string::npos);
    EXPECT_NE(content.find("key1"), std::string::npos);
    EXPECT_NE(content.find("key2"), std::string::npos);
}

TEST_F(AofManagerTest, RewriteListKeysAndReplay) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    credis::store::Store store;
    store.rpush("mylist", "a");
    store.rpush("mylist", "b");

    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.open(tmp_dir_path_);

    ASSERT_TRUE(aof.start_rewrite(store, tmp_dir_path_));

    while (!aof.check_rewrite_complete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    aof.close();

    auto content = aof.read_aof_content(tmp_dir_path_);
    EXPECT_NE(content.find("RPUSH"), std::string::npos);
    EXPECT_NE(content.find("mylist"), std::string::npos);
    EXPECT_NE(content.find('a'), std::string::npos);
    EXPECT_NE(content.find('b'), std::string::npos);
}

TEST_F(AofManagerTest, RewriteZsetKeysAndReplay) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    credis::store::Store store;
    store.zadd("z", 1.0, "a");
    store.zadd("z", 2.0, "b");

    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.open(tmp_dir_path_);

    ASSERT_TRUE(aof.start_rewrite(store, tmp_dir_path_));

    while (!aof.check_rewrite_complete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    aof.close();

    auto content = aof.read_aof_content(tmp_dir_path_);
    EXPECT_NE(content.find("ZADD"), std::string::npos);
    EXPECT_NE(content.find('z'), std::string::npos);
}

TEST_F(AofManagerTest, RewriteAlreadyInProgressReturnsFalse) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    credis::store::Store store;
    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.open(tmp_dir_path_);

    ASSERT_TRUE(aof.start_rewrite(store, tmp_dir_path_));
    EXPECT_FALSE(aof.start_rewrite(store, tmp_dir_path_));

    while (!aof.check_rewrite_complete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    aof.close();
}

TEST_F(AofManagerTest, RewriteWithConcurrentWriteIncludesBuffer) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    credis::store::Store store;
    store.set("k1", "before");

    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.open(tmp_dir_path_);

    ASSERT_TRUE(aof.start_rewrite(store, tmp_dir_path_));
    aof.append_to_rewrite_buffer("*3\r\n$3\r\nSET\r\n$2\r\nk2\r\n$1\r\n2\r\n");

    while (!aof.check_rewrite_complete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    aof.close();

    auto content = aof.read_aof_content(tmp_dir_path_);
    EXPECT_NE(content.find("k2"), std::string::npos);
}

TEST_F(AofManagerTest, RewriteStreamKeysAndReplay) {
    auto dir_path = tmp_dir_path_ + "/subdir";
    std::filesystem::create_directories(dir_path);
    {
        std::ofstream mf(dir_path + "/myapp.aof.manifest");
        mf << "file myapp.aof.1.incr.aof seq 1 type i\n";
    }
    {
        std::ofstream af(dir_path + "/myapp.aof.1.incr.aof");
    }

    credis::store::Store store;
    store.xadd("s", "1-0", {{"f1", "v1"}});
    store.xadd("s", "2-0", {{"f2", "v2"}});

    AofManager aof;
    aof.set_appendonly("yes");
    aof.set_appenddirname("subdir");
    aof.set_appendfilename("myapp.aof");
    aof.open(tmp_dir_path_);

    ASSERT_TRUE(aof.start_rewrite(store, tmp_dir_path_));

    while (!aof.check_rewrite_complete()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    aof.close();

    auto content = aof.read_aof_content(tmp_dir_path_);
    EXPECT_NE(content.find("XADD"), std::string::npos);
    EXPECT_NE(content.find('s'), std::string::npos);
    EXPECT_NE(content.find("1-0"), std::string::npos);
    EXPECT_NE(content.find("2-0"), std::string::npos);
}
