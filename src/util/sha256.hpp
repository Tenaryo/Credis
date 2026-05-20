#pragma once

#include <string>
#include <string_view>

namespace credis::util {

auto sha256(std::string_view input) -> std::string;

} // namespace credis::util
