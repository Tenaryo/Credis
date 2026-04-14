#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "util/sha256.hpp"

struct AclUser {
    std::vector<std::string> passwords;
    bool nopass{true};
};

class AclManager {
    std::unordered_map<std::string, AclUser> users_;
  public:
    AclManager() { users_.emplace("default", AclUser{}); }

    auto get_user(std::string_view username) const -> const AclUser* {
        auto it = users_.find(std::string(username));
        return it != users_.end() ? &it->second : nullptr;
    }

    auto set_password(std::string_view username, std::string_view password) -> void {
        auto& user = users_[std::string(username)];
        user.passwords.push_back(util::sha256(password));
        user.nopass = false;
    }

    auto authenticate(std::string_view username, std::string_view password) const -> bool {
        auto* user = get_user(username);
        if (!user)
            return false;
        auto hash = util::sha256(password);
        return std::ranges::find(user->passwords, hash) != user->passwords.end();
    }
};
