#include "core/string_utils.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace oop::str {

std::string trim(std::string_view value) {
    auto begin = value.begin();
    auto end = value.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

std::string to_lower(std::string_view value) {
    std::string out(value);
    std::ranges::transform(out, out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::vector<std::string> split_words(std::string_view value) {
    std::vector<std::string> words;
    std::string current;
    for (const char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            current.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else if (!current.empty()) {
            words.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) {
        words.push_back(current);
    }
    return words;
}

bool contains_ci(std::string_view haystack, std::string_view needle) {
    return to_lower(haystack).find(to_lower(needle)) != std::string::npos;
}

std::string replace_all(std::string value, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return value;
    }
    std::size_t pos = 0;
    while ((pos = value.find(from, pos)) != std::string::npos) {
        value.replace(pos, from.size(), to);
        pos += to.size();
    }
    return value;
}

}  // namespace oop::str
