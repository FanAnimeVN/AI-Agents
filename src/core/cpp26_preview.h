#pragma once

#include <optional>
#include <version>

namespace oop::cpp26_preview {

#if defined(__cpp_lib_optional_range_support) && __cpp_lib_optional_range_support >= 202406L

// C++26 standard-library path: optional is a zero-or-one range.
template <class T>
const std::optional<T>& one_or_empty(const std::optional<T>& value) {
    return value;
}

#else

template <class T>
class OptionalRefView {
public:
    explicit OptionalRefView(const T* value) : value_(value) {}

    [[nodiscard]] const T* begin() const {
        return value_;
    }

    [[nodiscard]] const T* end() const {
        return value_ == nullptr ? nullptr : value_ + 1;
    }

private:
    const T* value_ = nullptr;
};

// C++26 standardizes optional as a zero-or-one range. Current standard
// libraries are not consistent yet, so this adapter gives the same iteration
// behavior while keeping the project buildable on C++23 and C++26 compilers.
template <class T>
auto one_or_empty(const std::optional<T>& value) {
    return OptionalRefView<T>(value.has_value() ? &*value : nullptr);
}

#endif

}  // namespace oop::cpp26_preview
