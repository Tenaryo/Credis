#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

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
