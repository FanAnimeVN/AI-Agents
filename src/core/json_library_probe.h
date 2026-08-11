#pragma once

#include <string>
#include <string_view>

namespace oop {

std::string json_backend_name();
bool nlohmann_json_accepts(std::string_view text);

}  // namespace oop
