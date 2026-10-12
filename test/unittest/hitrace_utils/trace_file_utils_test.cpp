/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <unistd.h>
#include <vector>
#include "trace_file_utils.h"
#include "common_utils.h"
#include "smart_fd.h"

using namespace testing::ext;
using namespace std;

namespace OHOS {
namespace HiviewDFX {
class TraceFileUtilsTest : public testing::Test {
public:
    void SetUp() override
    {
        FILE* fp = fopen("/data/local/tmp/iswritable_test_file.txt", "w");
        if (fp != nullptr) {
            fclose(fp);
        }
        constexpr auto mode = 0755;
        mkdir("/data/local/tmp/subdir", mode);
        mkdir("/data/local/tmpX_test", mode);
    }
    void TearDown() override
    {
        (void)remove("/data/local/tmp/iswritable_test_file.txt");
        rmdir("/data/local/tmp/subdir");
        rmdir("/data/local/tmpX_test");
    }
};

/**
 * @tc.name: TraverseFiles01
 * @tc.desc: Test TraverseFiles(), enter an existing file path.
 * @tc.type: FUNC
 */
HWTEST_F(TraceFileUtilsTest, TraverseFiles01, TestSize.Level2)
{
    EXPECT_FALSE(Hitrace::TraverseFiles("///", false, nullptr));
    EXPECT_FALSE(Hitrace::TraverseFiles("", false, nullptr));
    constexpr auto testPath = "/data/test/";
    EXPECT_TRUE(Hitrace::TraverseFiles("/data/test/", true,
        [testPath] (const char * directory, const dirent* item) {
            EXPECT_EQ(strncmp(directory, testPath, strlen(testPath) - 1), 0);
            EXPECT_NE(item->d_name, nullptr);
        }));
}

/**
 * @tc.name: IsWritable01
 * @tc.desc: Test IsWritable() with paths covering realpath-fail branches via parentDir resolution.
 * @tc.type: FUNC
 */
HWTEST_F(TraceFileUtilsTest, IsWritable01, TestSize.Level2)
{
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp"));
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp/test.txt"));
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp/"));

    ASSERT_FALSE(Hitrace::IsWritable("/system/bin/test.txt"));
    ASSERT_FALSE(Hitrace::IsWritable("/data/local/tmp/../test.txt"));
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp/./test.txt"));
    ASSERT_FALSE(Hitrace::IsWritable("/data/local/tmp/.."));
}

/**
 * @tc.name: IsWritable04
 * @tc.desc: Test IsWritable() with existing file under writable path, non-existent file in
 *           writable subdirectory, path with prefix collision, and filename containing "..".
 * @tc.type: FUNC
 */
HWTEST_F(TraceFileUtilsTest, IsWritable04, TestSize.Level2)
{
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp/iswritable_test_file.txt"));
    ASSERT_FALSE(Hitrace::IsWritable("/data/local/tmpX_test"));
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp/subdir/nonexist.txt"));
    ASSERT_TRUE(Hitrace::IsWritable("/data/local/tmp/..hidden"));
}

/**
 * @tc.name: IsWritable05
 * @tc.desc: Test IsWritable() with symbolic link paths.
 *           realpath resolves symlinks to their target before prefix check.
 * @tc.type: FUNC
 */
HWTEST_F(TraceFileUtilsTest, IsWritable05, TestSize.Level2)
{
    const char* linkToWritable = "/data/local/tmp/symlink_to_writable";
    EXPECT_EQ(symlink("/data/local/tmp/iswritable_test_file.txt", linkToWritable), 0);
    EXPECT_TRUE(Hitrace::IsWritable(linkToWritable));

    const char* linkToSystem = "/data/local/tmp/symlink_to_system";
    EXPECT_EQ(symlink("/system/bin", linkToSystem), 0);
    EXPECT_FALSE(Hitrace::IsWritable(linkToSystem));

    const char* danglingLink = "/data/local/tmp/dangling_symlink";
    EXPECT_EQ(symlink("/data/local/tmp/nonexist_target.txt", danglingLink), 0);
    EXPECT_TRUE(Hitrace::IsWritable(danglingLink));

    unlink(linkToWritable);
    unlink(linkToSystem);
    unlink(danglingLink);
}

/**
 * @tc.name: IsWritableDir01
 * @tc.desc: Test IsWritableDir(), enter an existing file path.
 * @tc.type: FUNC
 */
HWTEST_F(TraceFileUtilsTest, IsWritableDir01, TestSize.Level2)
{
    ASSERT_TRUE(Hitrace::IsWritableDir("/data/local/tmp"));
    ASSERT_TRUE(Hitrace::IsWritableDir("/data/local/tmp/"));

    ASSERT_FALSE(Hitrace::IsWritableDir("/data/local/tmp/test.txt"));
    ASSERT_FALSE(Hitrace::IsWritableDir("/system/bin"));
}

/**
 * @tc.name: GenerateSnapshotInDefaultDirectory
 * @tc.desc: Generate a snapshot filename for either spelling of the default directory.
 * @tc.type: FUNC
 */
HWTEST_F(TraceFileUtilsTest, GenerateSnapshotInDefaultDirectory, TestSize.Level2)
{
    const std::string prefix = "/data/log/hitrace/trace_";
    for (const std::string path : {"/data/log/hitrace", "/data/log/hitrace/"}) {
        const auto file = Hitrace::GenerateTraceFileName(Hitrace::TRACE_SNAPSHOT, path);
        EXPECT_EQ(file.compare(0, prefix.size(), prefix), 0) << path;
        ASSERT_GT(file.size(), prefix.size()) << path;
        EXPECT_EQ(file.substr(file.size() - 4), ".sys");
    }
}

/**
 * @tc.name: RejectSnapshotOutsideDefaultDirectory
 * @tc.desc: The default directory exception does not allow arbitrary paths or traversal.
 * @tc.type: FUNC
 */
HWTEST_F(TraceFileUtilsTest, RejectSnapshotOutsideDefaultDirectory, TestSize.Level2)
{
    for (const std::string path : {"/data/log/hitrace-other", "/data/log/hitrace/../outside.sys",
        "/data/log/hitrace/custom.sys", "/data/log", "/system/bin"}) {
        EXPECT_TRUE(Hitrace::GenerateTraceFileName(Hitrace::TRACE_SNAPSHOT, path).empty()) << path;
    }
}

class CacheTraceFileSizeTest : public testing::Test {
protected:
    static constexpr int64_t fileSize = 10;

    void SetUp() override
    {
        char directory[] = "/data/local/tmp/hitrace_cache_size_XXXXXX";
        ASSERT_NE(mkdtemp(directory), nullptr);
        directory_ = directory;
    }

    void TearDown() override
    {
        for (auto it = createdFiles_.rbegin(); it != createdFiles_.rend(); ++it) {
            (void)remove(it->c_str());
        }
        if (!directory_.empty()) {
            (void)rmdir(directory_.c_str());
        }
    }

    std::string Path(const std::string& name) const
    {
        return directory_ + "/" + name;
    }

    void ExpectSource(const std::string& path, const std::string& expected)
    {
        char source[16] = {};
        ASSERT_EQ(getxattr(path.c_str(), "user.trace_source", source, sizeof(source)),
            static_cast<ssize_t>(expected.size()));
        EXPECT_STREQ(source, expected.c_str());
    }

    bool AddCacheFile(const std::string& name)
    {
        const auto path = Path(name);
        FILE* file = fopen(path.c_str(), "w");
        if (file == nullptr) {
            return false;
        }
        createdFiles_.push_back(path);
        const bool written = fputs("0123456789", file) >= 0;
        const bool closed = fclose(file) == 0;
        if (!written || !closed) {
            return false;
        }
        Hitrace::TraceFileInfo info;
        info.filename = path;
        info.fileSize = fileSize;
        cacheFiles_.push_back(info);
        return true;
    }

    bool AddCacheFiles(const std::string& names)
    {
        for (char name : names) {
            if (!AddCacheFile(std::string(1, name))) {
                return false;
            }
        }
        return true;
    }

    void ExpectFiles(const std::string& names)
    {
        std::vector<std::string> expected;
        for (char name : names) {
            expected.push_back(Path(std::string(1, name)));
        }
        std::vector<std::string> actual;
        for (const auto& file : cacheFiles_) {
            actual.push_back(file.filename);
        }
        EXPECT_EQ(actual, expected);
        for (char name = 'A'; name <= 'E'; ++name) {
            EXPECT_EQ(access(Path(std::string(1, name)).c_str(), F_OK) == 0,
                names.find(name) != std::string::npos) << name;
        }
    }

    std::string directory_;
    std::vector<std::string> createdFiles_;
    std::vector<Hitrace::TraceFileInfo> cacheFiles_;
};

HWTEST_F(CacheTraceFileSizeTest, AtLimitRetainsFiles, TestSize.Level1)
{
    ASSERT_TRUE(AddCacheFiles("ABCD"));
    Hitrace::ClearCacheTraceFileBySize(cacheFiles_, 4 * fileSize);
    ExpectFiles("ABCD");
}

HWTEST_F(CacheTraceFileSizeTest, MissingOldestDoesNotEvictValidFiles, TestSize.Level1)
{
    ASSERT_TRUE(AddCacheFiles("ABCD"));
    ASSERT_EQ(remove(Path("A").c_str()), 0);
    ASSERT_TRUE(AddCacheFile("E"));
    Hitrace::ClearCacheTraceFileBySize(cacheFiles_, 4 * fileSize);
    ExpectFiles("BCDE");
}

HWTEST_F(CacheTraceFileSizeTest, MissingNewestDoesNotEvictOldest, TestSize.Level1)
{
    ASSERT_TRUE(AddCacheFiles("ABCDE"));
    ASSERT_EQ(remove(Path("E").c_str()), 0);
    Hitrace::ClearCacheTraceFileBySize(cacheFiles_, 4 * fileSize);
    ExpectFiles("ABCD");
}

HWTEST_F(CacheTraceFileSizeTest, MissingEntriesRemovedBelowLimit, TestSize.Level1)
{
    ASSERT_TRUE(AddCacheFiles("ABC"));
    ASSERT_EQ(remove(Path("A").c_str()), 0);
    ASSERT_EQ(remove(Path("C").c_str()), 0);
    Hitrace::ClearCacheTraceFileBySize(cacheFiles_, 4 * fileSize);
    ExpectFiles("B");
}

HWTEST_F(CacheTraceFileSizeTest, PromotedFileLeavesCacheBudget, TestSize.Level1)
{
    ASSERT_TRUE(AddCacheFile("cache_trace_A"));
    ASSERT_TRUE(AddCacheFiles("BCD"));
    const auto promoted = Hitrace::RenameCacheFile(Path("cache_trace_A"), Hitrace::TraceFileSource::SYSTEM);
    createdFiles_.push_back(promoted);
    ASSERT_EQ(promoted, Path("trace_A"));
    Hitrace::ClearCacheTraceFileBySize(cacheFiles_, 3 * fileSize);
    ExpectFiles("BCD");
    EXPECT_EQ(access(promoted.c_str(), F_OK), 0);
}

HWTEST_F(CacheTraceFileSizeTest, OverLimitEvictsOldestOnly, TestSize.Level1)
{
    ASSERT_TRUE(AddCacheFiles("ABCD"));
    Hitrace::ClearCacheTraceFileBySize(cacheFiles_, 2 * fileSize);
    ExpectFiles("CD");
}

HWTEST_F(CacheTraceFileSizeTest, FailedDeletionRemainsTrackedForRetry, TestSize.Level1)
{
    // A nonempty directory makes remove fail even when the test runs as root.
    ASSERT_EQ(mkdir(Path("A").c_str(), 0700), 0);
    createdFiles_.push_back(Path("A"));
    Hitrace::TraceFileInfo blocked;
    blocked.filename = Path("A");
    blocked.fileSize = fileSize;
    cacheFiles_.push_back(blocked);
    ASSERT_TRUE(AddCacheFile("A/child"));
    cacheFiles_.pop_back();
    ASSERT_TRUE(AddCacheFiles("BC"));

    Hitrace::ClearCacheTraceFileBySize(cacheFiles_, 2 * fileSize);
    ExpectFiles("AC");

    ASSERT_EQ(remove(Path("A/child").c_str()), 0);
    Hitrace::ClearCacheTraceFileBySize(cacheFiles_, fileSize);
    ExpectFiles("C");
}

HWTEST_F(CacheTraceFileSizeTest, ZeroLimitAndEmptyCache, TestSize.Level1)
{
    ASSERT_TRUE(AddCacheFiles("AB"));
    Hitrace::ClearCacheTraceFileBySize(cacheFiles_, 0);
    ExpectFiles("");
    Hitrace::ClearCacheTraceFileBySize(cacheFiles_, 0);
    ExpectFiles("");
}

HWTEST_F(CacheTraceFileSizeTest, ConversionUsesRequestSourceAndPreservesMtime, TestSize.Level1)
{
    for (const auto source : {Hitrace::TraceFileSource::USER, Hitrace::TraceFileSource::SYSTEM}) {
        ASSERT_TRUE(AddCacheFile("cache_trace_A"));
        const auto cache = Path("cache_trace_A");
        ASSERT_EQ(setxattr(cache.c_str(), "user.trace_source", "system", 6, 0), 0);
        timespec times[2] = {{1, 0}, {1, 0}};
        ASSERT_EQ(utimensat(AT_FDCWD, cache.c_str(), times, 0), 0);
        const auto snapshot = Hitrace::RenameCacheFile(cache, source);
        createdFiles_.push_back(snapshot);
        ASSERT_EQ(snapshot, Path("trace_A"));
        ExpectSource(snapshot, source == Hitrace::TraceFileSource::USER ? "user" : "system");
        struct stat info {};
        ASSERT_EQ(stat(snapshot.c_str(), &info), 0);
        EXPECT_EQ(info.st_mtim.tv_sec, 1);
        EXPECT_EQ(info.st_mtim.tv_nsec, 0);
        EXPECT_NE(access(cache.c_str(), F_OK), 0);
        ASSERT_EQ(remove(snapshot.c_str()), 0);
        cacheFiles_.clear();
    }
}

HWTEST_F(CacheTraceFileSizeTest, FailedConversionRestoresCacheSource, TestSize.Level1)
{
    ASSERT_TRUE(AddCacheFile("cache_trace_A"));
    const auto cache = Path("cache_trace_A");
    ASSERT_EQ(setxattr(cache.c_str(), "user.trace_source", "system", 6, 0), 0);
    ASSERT_EQ(mkdir(Path("trace_A").c_str(), 0700), 0);
    createdFiles_.push_back(Path("trace_A"));
    EXPECT_TRUE(Hitrace::RenameCacheFile(cache, Hitrace::TraceFileSource::USER).empty());
    EXPECT_EQ(access(cache.c_str(), F_OK), 0);
    ExpectSource(cache, "system");
}

HWTEST_F(CacheTraceFileSizeTest, InvalidSourceDoesNotReplaceExistingTag, TestSize.Level1)
{
    ASSERT_TRUE(AddCacheFile("A"));
    SmartFd fd(open(Path("A").c_str(), O_RDONLY));
    ASSERT_TRUE(fd);
    ASSERT_TRUE(Hitrace::SetTraceFileSource(fd.GetFd(), Hitrace::TraceFileSource::USER));
    EXPECT_FALSE(Hitrace::SetTraceFileSource(fd.GetFd(), static_cast<Hitrace::TraceFileSource>(255)));
    ExpectSource(Path("A"), "user");
}

} // namespace HiviewDFX
} // namespace OHOS
