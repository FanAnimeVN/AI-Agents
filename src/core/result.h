#pragma once

#include <expected>
#include <string>

namespace oop {

template <class T>
using Result = std::expected<T, std::string>;

inline Result<void> ok() {
    return {};
}

template <class T>
Result<T> fail(std::string message) {
    return std::unexpected(std::move(message));
}

}  // namespace oop
