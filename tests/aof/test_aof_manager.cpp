#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

#include "aof/aof_manager.hpp"

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

TEST_F(AofManagerTest, EverysecFsyncThreadStartsAndStops) {
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
    aof.set_appendfsync("everysec");
    aof.open(tmp_dir_path_);

    EXPECT_EQ(aof.appendfsync(), "everysec");

    aof.close();
}

TEST_F(AofManagerTest, EverysecFsyncAppendDoesNotCrash) {
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
    aof.set_appendfsync("everysec");
    aof.open(tmp_dir_path_);

    aof.append("*3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\n100\r\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    aof.close();
}

TEST_F(AofManagerTest, AppendfsyncSwitchFromEverysecToAlways) {
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
    aof.set_appendfsync("everysec");
    aof.open(tmp_dir_path_);

    aof.set_appendfsync("always");
    EXPECT_EQ(aof.appendfsync(), "always");

    aof.close();
}

TEST_F(AofManagerTest, AppendfsyncSwitchFromAlwaysToNo) {
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
    aof.set_appendfsync("always");
    aof.open(tmp_dir_path_);

    aof.set_appendfsync("no");
    EXPECT_EQ(aof.appendfsync(), "no");

    aof.close();
}

TEST_F(AofManagerTest, AppendfsyncSwitchToEverysecStartsThread) {
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
    aof.set_appendfsync("no");
    aof.open(tmp_dir_path_);

    aof.set_appendfsync("everysec");
    EXPECT_EQ(aof.appendfsync(), "everysec");

    aof.close();
}
