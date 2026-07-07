#pragma once

#include "core/result.h"

#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace oop {

class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json>;
    using Value = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;

    Json();
    Json(std::nullptr_t);
    Json(bool value);
    Json(int value);
    Json(long long value);
    Json(double value);
    Json(const char* value);
    Json(std::string value);
    Json(Array value);
    Json(Object value);

    static Json object();
    static Json array();
    static Result<Json> parse(std::string_view text);

    [[nodiscard]] bool is_null() const;
    [[nodiscard]] bool is_bool() const;
    [[nodiscard]] bool is_number() const;
    [[nodiscard]] bool is_string() const;
    [[nodiscard]] bool is_array() const;
    [[nodiscard]] bool is_object() const;

    [[nodiscard]] bool as_bool(bool fallback = false) const;
    [[nodiscard]] double as_number(double fallback = 0.0) const;
    [[nodiscard]] const std::string& as_string() const;
    [[nodiscard]] std::string as_string_or(std::string fallback = {}) const;
    [[nodiscard]] const Array& as_array() const;
    [[nodiscard]] const Object& as_object() const;

    Array& array_items();
    Object& object_items();

    [[nodiscard]] bool contains(std::string_view key) const;
    [[nodiscard]] const Json& at(std::string_view key) const;
    [[nodiscard]] const Json& at(std::size_t index) const;
    Json& operator[](std::string key);
    Json& operator[](std::size_t index);

    void push_back(Json value);

    [[nodiscard]] std::string dump(int indent = -1) const;

private:
    Value value_;
};

std::string json_escape(std::string_view value);

}  // namespace oop
