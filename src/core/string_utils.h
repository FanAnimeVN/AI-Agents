#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace oop::str {

std::string trim(std::string_view value);
std::string to_lower(std::string_view value);
std::vector<std::string> split_words(std::string_view value);
bool contains_ci(std::string_view haystack, std::string_view needle);
std::string replace_all(std::string value, std::string_view from, std::string_view to);

}  // namespace oop::str
