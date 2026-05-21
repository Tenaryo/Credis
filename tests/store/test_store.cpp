#include <gtest/gtest.h>

#include <chrono>
#include <thread>

#include "store/store.hpp"

using namespace credis::store;

// ── String tests ──────────────────────────────────────────────────────

TEST(StoreTest, SetStoresValueGetRetrievesIt) {
    Store store;
    store.set("key1", "hello");
    auto result = store.get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "hello");
}

TEST(StoreTest, SetOverwritesExisting) {
    Store store;
    store.set("key1", "hello");
    store.set("key1", "world");
    auto result = store.get("key1");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, "world");
}

TEST(StoreTest, GetNonexistentKeyReturnsNullopt) {
    Store store;
    auto result = store.get("nonexistent");
    EXPECT_FALSE(result.has_value());
}

TEST(StoreTest, IncrIncrementsPositiveInteger) {
    Store store;
    store.set("counter", "5");
    auto result = store.incr("counter");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 6);
    auto val = store.get("counter");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "6");
}

TEST(StoreTest, IncrIncrementsNegativeInteger) {
    Store store;
    store.set("counter", "-3");
    auto result = store.incr("counter");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, -2);
    auto val = store.get("counter");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "-2");
}

TEST(StoreTest, IncrIncrementsZeroToOne) {
    Store store;
    store.set("counter", "0");
    auto result = store.incr("counter");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
    auto val = store.get("counter");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "1");
}

TEST(StoreTest, IncrOnNonexistentKeyCreatesWithValue1) {
    Store store;
    auto result = store.incr("nonexistent");
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 1);
    auto val = store.get("nonexistent");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "1");
}

TEST(StoreTest, IncrOnNonIntegerReturnsNullopt) {
    Store store;
    store.set("key", "abc");
    auto result = store.incr("key");
    EXPECT_FALSE(result.has_value());
}

TEST(StoreTest, DelRemovesKeyExistsReturnsFalse) {
    Store store;
    store.set("key1", "value");
    EXPECT_TRUE(store.del("key1"));
    EXPECT_FALSE(store.exists("key1"));
}

TEST(StoreTest, ExistsReturnsTrueForExistingFalseForMissing) {
    Store store;
    store.set("existing", "val");
    EXPECT_TRUE(store.exists("existing"));
    EXPECT_FALSE(store.exists("missing"));
}

TEST(StoreTest, TypeReturnsStringForStringValues) {
    Store store;
    store.set("key", "value");
    EXPECT_EQ(store.get_type("key"), "string");
}

// ── List tests ────────────────────────────────────────────────────────

TEST(StoreTest, RpushAppendsLlenReturnsCountLrangeReturnsValues) {
    Store store;
    store.rpush("mylist", "a");
    store.rpush("mylist", "b");
    store.rpush("mylist", "c");
    EXPECT_EQ(store.llen("mylist"), 3);
    auto values = store.lrange("mylist", 0, 2);
    ASSERT_EQ(values.size(), 3uz);
    EXPECT_EQ(values[0], "a");
    EXPECT_EQ(values[1], "b");
    EXPECT_EQ(values[2], "c");
}

TEST(StoreTest, LpushPrependsLrangeWithNegativeStop) {
    Store store;
    store.lpush("mylist", "x");
    store.lpush("mylist", "y");
    auto values = store.lrange("mylist", 0, -1);
    ASSERT_EQ(values.size(), 2uz);
    EXPECT_EQ(values[0], "y");
    EXPECT_EQ(values[1], "x");
}

TEST(StoreTest, LpopRemovesAndReturnsLeftmostElement) {
    Store store;
    store.rpush("mylist", "first");
    store.rpush("mylist", "second");
    auto popped = store.lpop("mylist");
    ASSERT_TRUE(popped.has_value());
    EXPECT_EQ(*popped, "first");
    EXPECT_EQ(store.llen("mylist"), 1);
}

TEST(StoreTest, TypeReturnsListForListValues) {
    Store store;
    store.rpush("mylist", "item");
    EXPECT_EQ(store.get_type("mylist"), "list");
}

// ── Stream tests ──────────────────────────────────────────────────────

TEST(StoreTest, XaddWithStarAutoGeneratesIdXreadReturnsEntry) {
    Store store;
    auto id = store.xadd("mystream", "*", {{"field1", "value1"}});
    EXPECT_FALSE(id.empty());
    auto span = store.xread("mystream", "0-0");
    std::vector<StreamEntry> entries(span.begin(), span.end());
    ASSERT_FALSE(entries.empty());
    EXPECT_EQ(entries[0].id, id);
}

TEST(StoreTest, XaddAutoGeneratesIncrementingSequenceNumbers) {
    Store store;
    store.xadd("mystream", "*", {{"f1", "v1"}});
    store.xadd("mystream", "*", {{"f2", "v2"}});
    auto span = store.xrange("mystream", "-", "+");
    std::vector<StreamEntry> entries(span.begin(), span.end());
    ASSERT_EQ(entries.size(), 2uz);
    auto id1 = credis::protocol::StreamId::parse(entries[0].id);
    auto id2 = credis::protocol::StreamId::parse(entries[1].id);
    ASSERT_TRUE(id1.has_value());
    ASSERT_TRUE(id2.has_value());
    EXPECT_LT(id1->sequence, id2->sequence);
}

TEST(StoreTest, XrangeReturnsEntriesInRangeWithDashPlusSentinel) {
    Store store;
    store.xadd("mystream", "1000-0", {{"a", "1"}});
    store.xadd("mystream", "2000-0", {{"b", "2"}});
    store.xadd("mystream", "3000-0", {{"c", "3"}});
    auto span = store.xrange("mystream", "-", "+");
    std::vector<StreamEntry> entries(span.begin(), span.end());
    ASSERT_EQ(entries.size(), 3uz);
}

TEST(StoreTest, XrangeWithExplicitStartEndPartialMatch) {
    Store store;
    store.xadd("mystream", "1000-0", {{"a", "1"}});
    store.xadd("mystream", "2000-0", {{"b", "2"}});
    store.xadd("mystream", "2000-1", {{"c", "3"}});
    auto span = store.xrange("mystream", "2000", "2000");
    std::vector<StreamEntry> entries(span.begin(), span.end());
    ASSERT_EQ(entries.size(), 2uz);
}

TEST(StoreTest, XreadReturnsAfterIdEmptyWhenNoNewEntries) {
    Store store;
    store.xadd("mystream", "1000-0", {{"k", "v"}});
    auto max_id = store.get_stream_max_id("mystream");
    ASSERT_TRUE(max_id.has_value());
    auto span = store.xread("mystream", *max_id);
    std::vector<StreamEntry> entries(span.begin(), span.end());
    EXPECT_TRUE(entries.empty());
}

TEST(StoreTest, TypeReturnsStreamForStreamValues) {
    Store store;
    store.xadd("mystream", "*", {{"k", "v"}});
    EXPECT_EQ(store.get_type("mystream"), "stream");
}

// ── Sorted set tests ──────────────────────────────────────────────────

TEST(StoreTest, ZaddCreatesNewSortedSetReturns1) {
    Store store;
    auto result = store.zadd("zset", 1.0, "member1");
    EXPECT_EQ(result, 1);
}

TEST(StoreTest, ZaddUpdatesExistingMemberScoreReturns0) {
    Store store;
    store.zadd("zset", 1.0, "member1");
    auto result = store.zadd("zset", 2.0, "member1");
    EXPECT_EQ(result, 0);
}

TEST(StoreTest, ZrankReturnsCorrectZeroBasedRank) {
    Store store;
    store.zadd("zset", 1.0, "a");
    store.zadd("zset", 2.0, "b");
    store.zadd("zset", 3.0, "c");
    EXPECT_EQ(store.zrank("zset", "a"), 0);
    EXPECT_EQ(store.zrank("zset", "b"), 1);
    EXPECT_EQ(store.zrank("zset", "c"), 2);
}

TEST(StoreTest, ZscoreReturnsMemberScore) {
    Store store;
    store.zadd("zset", 42.5, "member1");
    auto score = store.zscore("zset", "member1");
    ASSERT_TRUE(score.has_value());
    EXPECT_DOUBLE_EQ(*score, 42.5);
}

TEST(StoreTest, ZrangeReturnsMembersInScoreOrder) {
    Store store;
    store.zadd("zset", 3.0, "c");
    store.zadd("zset", 1.0, "a");
    store.zadd("zset", 2.0, "b");
    auto members = store.zrange("zset", 0, -1);
    ASSERT_EQ(members.size(), 3uz);
    EXPECT_EQ(members[0], "a");
    EXPECT_EQ(members[1], "b");
    EXPECT_EQ(members[2], "c");
}

TEST(StoreTest, ZcardReturnsMemberCount) {
    Store store;
    store.zadd("zset", 1.0, "a");
    store.zadd("zset", 2.0, "b");
    store.zadd("zset", 3.0, "c");
    EXPECT_EQ(store.zcard("zset"), 3);
}

TEST(StoreTest, ZremRemovesMemberReturns1Returns0ForNonexistent) {
    Store store;
    store.zadd("zset", 1.0, "member1");
    EXPECT_EQ(store.zrem("zset", "member1"), 1);
    EXPECT_EQ(store.zrem("zset", "member1"), 0);
}

TEST(StoreTest, TypeReturnsZsetForSortedSetValues) {
    Store store;
    store.zadd("zset", 1.0, "member1");
    EXPECT_EQ(store.get_type("zset"), "zset");
}

// ── Keys tests ────────────────────────────────────────────────────────

TEST(StoreTest, KeysReturnsAllKeyNames) {
    Store store;
    store.set("a", "1");
    store.set("b", "2");
    store.rpush("c", "item");
    auto all_keys = store.keys();
    auto has = [&](const std::string& k) { return std::find(all_keys.begin(), all_keys.end(), k) != all_keys.end(); };
    EXPECT_TRUE(has("a"));
    EXPECT_TRUE(has("b"));
    EXPECT_TRUE(has("c"));
}

// ── Watch / version tracking tests ────────────────────────────────────

TEST(StoreTest, GetKeyVersionReturns0ForNonexistentKey) {
    Store store;
    EXPECT_EQ(store.get_key_version("nonexistent"), uint64_t{0});
}

TEST(StoreTest, SetIncrementsKeyVersion) {
    Store store;
    store.set("key", "value");
    auto v1 = store.get_key_version("key");
    EXPECT_GT(v1, uint64_t{0});
    store.set("key", "value2");
    auto v2 = store.get_key_version("key");
    EXPECT_GT(v2, v1);
}
