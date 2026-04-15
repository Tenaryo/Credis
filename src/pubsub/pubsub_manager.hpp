#pragma once

#include "util/string_hash.hpp"

#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

class PubSubManager {
    std::unordered_map<int, std::unordered_set<std::string>> subscriptions_;
    std::unordered_map<std::string, std::unordered_set<int>, StringHash, std::equal_to<>>
        channel_subscribers_;
  public:
    size_t subscribe(int fd, std::string channel) {
        channel_subscribers_[channel].insert(fd);
        subscriptions_[fd].insert(std::move(channel));
        return subscriptions_[fd].size();
    }

    size_t unsubscribe(int fd, std::string_view channel) {
        auto it = subscriptions_.find(fd);
        if (it == subscriptions_.end())
            return 0;
        if (auto cit = channel_subscribers_.find(channel); cit != channel_subscribers_.end()) {
            cit->second.erase(fd);
            if (cit->second.empty())
                channel_subscribers_.erase(cit);
        }
        it->second.erase(std::string(channel));
        size_t remaining = it->second.size();
        if (remaining == 0)
            subscriptions_.erase(it);
        return remaining;
    }

    void unsubscribe(int fd) {
        auto it = subscriptions_.find(fd);
        if (it == subscriptions_.end())
            return;
        for (const auto& channel : it->second) {
            if (auto cit = channel_subscribers_.find(channel); cit != channel_subscribers_.end()) {
                cit->second.erase(fd);
                if (cit->second.empty())
                    channel_subscribers_.erase(cit);
            }
        }
        subscriptions_.erase(it);
    }

    [[nodiscard]] bool is_subscribed(int fd) const noexcept {
        auto it = subscriptions_.find(fd);
        return it != subscriptions_.end() && !it->second.empty();
    }

    [[nodiscard]] size_t subscriber_count(std::string_view channel) const noexcept {
        auto it = channel_subscribers_.find(channel);
        return it != channel_subscribers_.end() ? it->second.size() : 0;
    }

    [[nodiscard]] const std::unordered_set<int>&
    get_subscribers(std::string_view channel) const noexcept {
        static const std::unordered_set<int> kEmpty;
        auto it = channel_subscribers_.find(channel);
        return it != channel_subscribers_.end() ? it->second : kEmpty;
    }
};
