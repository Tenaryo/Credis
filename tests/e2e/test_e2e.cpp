#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "blocking_manager/blocking_manager.hpp"
#include "handler/command_handler.hpp"
#include "protocol/resp_parser.hpp"
#include "pubsub/pubsub_manager.hpp"
#include "store/store.hpp"

using namespace credis;

namespace {

auto make_resp(const std::vector<std::string>& args) -> std::string {
    return protocol::encode_array(args);
}

} // namespace

TEST(E2EBasicReadWrite, FullLifecycle) {
    store::Store store;
    server::ServerConfig config;
    handler::CommandHandler handler(store, config);

    EXPECT_EQ(handler.process(make_resp({"SET", "foo", "bar"})), "+OK\r\n");
    EXPECT_EQ(handler.process(make_resp({"GET", "foo"})), "$3\r\nbar\r\n");
}

TEST(E2EBasicReadWrite, MultipleKeys) {
    store::Store store;
    server::ServerConfig config;
    handler::CommandHandler handler(store, config);

    handler.process(make_resp({"SET", "k1", "v1"}));
    handler.process(make_resp({"SET", "k2", "v2"}));

    auto keys_resp = handler.process(make_resp({"KEYS", "*"}));
    EXPECT_NE(keys_resp.find("$2\r\nk1"), std::string::npos);
    EXPECT_NE(keys_resp.find("$2\r\nk2"), std::string::npos);
}

TEST(E2EBasicReadWrite, Overwrite) {
    store::Store store;
    server::ServerConfig config;
    handler::CommandHandler handler(store, config);

    handler.process(make_resp({"SET", "x", "a"}));
    handler.process(make_resp({"SET", "x", "b"}));

    EXPECT_EQ(handler.process(make_resp({"GET", "x"})), "$1\r\nb\r\n");
}

TEST(E2EMultiExec, Transaction) {
    store::Store store;
    server::ServerConfig config;
    handler::CommandHandler handler(store, config);

    EXPECT_EQ(handler.process(make_resp({"MULTI"})), "+OK\r\n");
    EXPECT_EQ(handler.process(make_resp({"SET", "k", "v"})), "+QUEUED\r\n");
    EXPECT_EQ(handler.process(make_resp({"EXEC"})), "*1\r\n+OK\r\n");
}

TEST(E2EMultiExec, NestedMultiReturnsError) {
    store::Store store;
    server::ServerConfig config;
    handler::CommandHandler handler(store, config);

    handler.process(make_resp({"MULTI"}));
    auto nested = handler.process(make_resp({"MULTI"}));
    EXPECT_TRUE(nested.starts_with("-ERR"));
}

TEST(E2EMultiExec, EmptyTransaction) {
    store::Store store;
    server::ServerConfig config;
    handler::CommandHandler handler(store, config);

    handler.process(make_resp({"MULTI"}));
    EXPECT_EQ(handler.process(make_resp({"EXEC"})), "*0\r\n");
}

TEST(E2EPubSub, SubscribeAndReceiveMessage) {
    store::Store store;
    server::ServerConfig config;
    handler::CommandHandler handler(store, config);
    pubsub::PubSubManager pubsub;
    handler.set_pubsub_manager(pubsub);

    std::unordered_map<int, std::vector<std::string>> delivered;
    auto send_fn = [&](int fd, const std::string& msg) { delivered[fd].push_back(msg); };

    handler.process_with_fd(1, make_resp({"subscribe", "news"}), send_fn);

    auto result = handler.process_with_fd(2, make_resp({"PUBLISH", "news", "hello"}), send_fn);

    ASSERT_EQ(delivered.size(), 1);
    ASSERT_TRUE(delivered.contains(1));

    auto expected_msg = "*3\r\n$7\r\nmessage\r\n$4\r\nnews\r\n$5\r\nhello\r\n";
    EXPECT_EQ(delivered[1][0], expected_msg);

    auto* normal = std::get_if<handler::ProcessResult::Normal>(&result.state);
    ASSERT_NE(normal, nullptr);
    EXPECT_EQ(normal->response, ":1\r\n");
}

TEST(E2EBlocking, BlpopExistingListReturnsElement) {
    store::Store store;
    server::ServerConfig config;
    handler::CommandHandler handler(store, config);

    handler.process(make_resp({"RPUSH", "q", "item"}));
    auto result = handler.process_with_fd(1, make_resp({"BLPOP", "q", "0"}), nullptr);

    ASSERT_TRUE(std::holds_alternative<handler::ProcessResult::Normal>(result.state));
    EXPECT_EQ(std::get<handler::ProcessResult::Normal>(result.state).response, "*2\r\n$1\r\nq\r\n$4\r\nitem\r\n");
}

TEST(E2ETypeValidation, SetThenZaddWrontype) {
    store::Store store;
    server::ServerConfig config;
    handler::CommandHandler handler(store, config);

    handler.process(make_resp({"SET", "key", "hello"}));
    auto zadd_resp = handler.process(make_resp({"ZADD", "key", "1.0", "member"}));

    EXPECT_NE(zadd_resp.find("WRONGTYPE"), std::string::npos);
}

TEST(E2ETypeValidation, ZaddThenSetWrontype) {
    store::Store store;
    server::ServerConfig config;
    handler::CommandHandler handler(store, config);

    handler.process(make_resp({"ZADD", "key", "1.0", "member"}));
    auto set_resp = handler.process(make_resp({"SET", "key", "hello"}));

    EXPECT_NE(set_resp.find("WRONGTYPE"), std::string::npos);
}

TEST(E2ERespRoundTrip, ParseEncodeParse) {
    auto original = std::vector<std::string>{"SET", "foo", "bar"};
    auto encoded = protocol::encode_array(original);
    auto parsed = protocol::parse_resp(encoded);

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, original);
}
