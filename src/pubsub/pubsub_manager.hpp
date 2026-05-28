#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "util/string_hash.hpp"

namespace credis::pubsub {

class PubSubManager {
    using ChannelSet = std::unordered_set<std::string, credis::util::StringHash, std::equal_to<>>;
    std::unordered_map<int, ChannelSet> subscriptions_;
    std::unordered_map<std::string, std::unordered_set<int>, credis::util::StringHash, std::equal_to<>>
        channel_subscribers_;

  public:
    auto subscribe(int fd, std::string channel) -> size_t {
        channel_subscribers_[channel].insert(fd);
        subscriptions_[fd].insert(std::move(channel));
        return subscriptions_[fd].size();
    }

    auto unsubscribe(int fd, std::string_view channel) -> size_t {
        auto it = subscriptions_.find(fd);
        if (it == subscriptions_.end()) [[unlikely]] {
            return 0;
        }
        if (auto cit = channel_subscribers_.find(channel); cit != channel_subscribers_.end()) [[likely]] {
            cit->second.erase(fd);
            if (cit->second.empty()) {
                channel_subscribers_.erase(cit);
            }
        }
        if (auto scit = it->second.find(channel); scit != it->second.end()) [[likely]] {
            it->second.erase(scit);
        }
        size_t remaining = it->second.size();
        if (remaining == 0) {
            subscriptions_.erase(it);
        }
        return remaining;
    }

    void unsubscribe(int fd) {
        auto it = subscriptions_.find(fd);
        if (it == subscriptions_.end()) {
            return;
        }
        for (const auto& channel : it->second) {
            if (auto cit = channel_subscribers_.find(channel); cit != channel_subscribers_.end()) [[likely]] {
                cit->second.erase(fd);
                if (cit->second.empty()) {
                    channel_subscribers_.erase(cit);
                }
            }
        }
        subscriptions_.erase(it);
    }

    [[nodiscard]] auto is_subscribed(int fd) const noexcept -> bool {
        auto it = subscriptions_.find(fd);
        return it != subscriptions_.end() && !it->second.empty();
    }

    [[nodiscard]] auto get_subscribers(std::string_view channel) const noexcept -> const std::unordered_set<int>& {
        static const std::unordered_set<int> kEmpty;
        auto it = channel_subscribers_.find(channel);
        return it != channel_subscribers_.end() ? it->second : kEmpty;
    }
};

} // namespace credis::pubsub
