#include "core/json.h"

#ifdef OOP_HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

#include <charconv>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace oop {
namespace {

const Json& null_json() {
    static const Json value;
    return value;
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    Result<Json> parse_document() {
        skip_ws();
        auto value = parse_value();
        if (!value) {
            return value;
        }
        skip_ws();
        if (pos_ != text_.size()) {
            return fail<Json>("Unexpected trailing characters in JSON");
        }
        return value;
    }

private:
    Result<Json> parse_value() {
        skip_ws();
        if (pos_ >= text_.size()) {
            return fail<Json>("Unexpected end of JSON");
        }
        const char c = text_[pos_];
        if (c == '"') {
            return parse_string();
        }
        if (c == '{') {
            return parse_object();
        }
        if (c == '[') {
            return parse_array();
        }
        if (text_.substr(pos_, 4) == "true") {
            pos_ += 4;
            return Json(true);
        }
        if (text_.substr(pos_, 5) == "false") {
            pos_ += 5;
            return Json(false);
        }
        if (text_.substr(pos_, 4) == "null") {
            pos_ += 4;
            return Json(nullptr);
        }
        return parse_number();
    }

    Result<Json> parse_string() {
        ++pos_;
        std::string out;
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') {
                return Json(out);
            }
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos_ >= text_.size()) {
                return fail<Json>("Invalid escape at end of string");
            }
            const char esc = text_[pos_++];
            switch (esc) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u':
                    if (pos_ + 4 > text_.size()) {
                        return fail<Json>("Incomplete unicode escape");
                    }
                    out.push_back('?');
                    pos_ += 4;
                    break;
                default:
                    return fail<Json>("Unsupported string escape");
            }
        }
        return fail<Json>("Unterminated string");
    }

    Result<Json> parse_number() {
        const std::size_t begin = pos_;
        if (text_[pos_] == '-') {
            ++pos_;
        }
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
                ++pos_;
            }
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
            }
        }
        double value = 0.0;
        const auto token = text_.substr(begin, pos_ - begin);
        const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
        if (ec != std::errc{} || ptr != token.data() + token.size()) {
            return fail<Json>("Invalid number in JSON");
        }
        return Json(value);
    }

    Result<Json> parse_array() {
        ++pos_;
        Json::Array out;
        skip_ws();
        if (peek(']')) {
            ++pos_;
            Json result(std::move(out));
            return result;
        }
        while (true) {
            auto value = parse_value();
            if (!value) {
                return value;
            }
            out.push_back(std::move(*value));
            skip_ws();
            if (peek(']')) {
                ++pos_;
                Json result(std::move(out));
                return result;
            }
            if (!peek(',')) {
                return fail<Json>("Expected comma in JSON array");
            }
            ++pos_;
        }
    }

    Result<Json> parse_object() {
        ++pos_;
        Json::Object out;
        skip_ws();
        if (peek('}')) {
            ++pos_;
            Json result(std::move(out));
            return result;
        }
        while (true) {
            skip_ws();
            if (!peek('"')) {
                return fail<Json>("Expected object key string");
            }
            auto key = parse_string();
            if (!key) {
                return key;
            }
            skip_ws();
            if (!peek(':')) {
                return fail<Json>("Expected colon after object key");
            }
            ++pos_;
            auto value = parse_value();
            if (!value) {
                return value;
            }
            out.emplace(key->as_string(), std::move(*value));
            skip_ws();
            if (peek('}')) {
                ++pos_;
                Json result(std::move(out));
                return result;
            }
            if (!peek(',')) {
                return fail<Json>("Expected comma in JSON object");
            }
            ++pos_;
        }
    }

    bool peek(char c) const {
        return pos_ < text_.size() && text_[pos_] == c;
    }

    void skip_ws() {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
        }
    }

    std::string_view text_;
    std::size_t pos_ = 0;
};

#ifdef OOP_HAVE_NLOHMANN_JSON
Json from_nlohmann(const nlohmann::json& value) {
    if (value.is_null()) {
        return Json(nullptr);
    }
    if (value.is_boolean()) {
        return Json(value.get<bool>());
    }
    if (value.is_number()) {
        return Json(value.get<double>());
    }
    if (value.is_string()) {
        return Json(value.get<std::string>());
    }
    if (value.is_array()) {
        Json::Array result;
        result.reserve(value.size());
        for (const auto& item : value) {
            result.push_back(from_nlohmann(item));
        }
        return Json(std::move(result));
    }

    Json::Object result;
    for (const auto& [key, item] : value.items()) {
        result.emplace(key, from_nlohmann(item));
    }
    return Json(std::move(result));
}
#endif

void dump_impl(const Json& value, std::ostringstream& out, int indent, int depth) {
    const auto newline = [&]() {
        if (indent >= 0) {
            out << '\n' << std::string(static_cast<std::size_t>((depth + 1) * indent), ' ');
        }
    };
    const auto close_indent = [&]() {
        if (indent >= 0) {
            out << '\n' << std::string(static_cast<std::size_t>(depth * indent), ' ');
        }
    };

    if (value.is_null()) {
        out << "null";
    } else if (value.is_bool()) {
        out << (value.as_bool() ? "true" : "false");
    } else if (value.is_number()) {
        const double number = value.as_number();
        if (std::isfinite(number) && std::floor(number) == number) {
            out << static_cast<long long>(number);
        } else {
            out << std::setprecision(15) << number;
        }
    } else if (value.is_string()) {
        out << '"' << json_escape(value.as_string()) << '"';
    } else if (value.is_array()) {
        out << '[';
        const auto& arr = value.as_array();
        for (std::size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) {
                out << ',';
            }
            newline();
            dump_impl(arr[i], out, indent, depth + 1);
        }
        if (!arr.empty()) {
            close_indent();
        }
        out << ']';
    } else {
        out << '{';
        const auto& obj = value.as_object();
        std::size_t i = 0;
        for (const auto& [key, child] : obj) {
            if (i++ > 0) {
                out << ',';
            }
            newline();
            out << '"' << json_escape(key) << '"' << (indent >= 0 ? ": " : ":");
            dump_impl(child, out, indent, depth + 1);
        }
        if (!obj.empty()) {
            close_indent();
        }
        out << '}';
    }
}

}  // namespace

Json::Json() : value_(nullptr) {}
Json::Json(std::nullptr_t) : value_(nullptr) {}
Json::Json(bool value) : value_(value) {}
Json::Json(int value) : value_(static_cast<double>(value)) {}
Json::Json(long long value) : value_(static_cast<double>(value)) {}
Json::Json(double value) : value_(value) {}
Json::Json(const char* value) : value_(std::string(value)) {}
Json::Json(std::string value) : value_(std::move(value)) {}
Json::Json(Array value) : value_(std::move(value)) {}
Json::Json(Object value) : value_(std::move(value)) {}

Json Json::object() {
    return Json(Object{});
}

Json Json::array() {
    return Json(Array{});
}

Result<Json> Json::parse(std::string_view text) {
#ifdef OOP_HAVE_NLOHMANN_JSON
    try {
        return from_nlohmann(nlohmann::json::parse(text.begin(), text.end()));
    } catch (const nlohmann::json::parse_error& error) {
        return fail<Json>("Invalid JSON: " + std::string(error.what()));
    } catch (const nlohmann::json::exception& error) {
        return fail<Json>("JSON error: " + std::string(error.what()));
    }
#else
    return Parser(text).parse_document();
#endif
}

bool Json::is_null() const { return std::holds_alternative<std::nullptr_t>(value_); }
bool Json::is_bool() const { return std::holds_alternative<bool>(value_); }
bool Json::is_number() const { return std::holds_alternative<double>(value_); }
bool Json::is_string() const { return std::holds_alternative<std::string>(value_); }
bool Json::is_array() const { return std::holds_alternative<Array>(value_); }
bool Json::is_object() const { return std::holds_alternative<Object>(value_); }

bool Json::as_bool(bool fallback) const {
    return is_bool() ? std::get<bool>(value_) : fallback;
}

double Json::as_number(double fallback) const {
    return is_number() ? std::get<double>(value_) : fallback;
}

const std::string& Json::as_string() const {
    if (!is_string()) {
        static const std::string empty;
        return empty;
    }
    return std::get<std::string>(value_);
}

std::string Json::as_string_or(std::string fallback) const {
    return is_string() ? std::get<std::string>(value_) : std::move(fallback);
}

const Json::Array& Json::as_array() const {
    if (!is_array()) {
        static const Array empty;
        return empty;
    }
    return std::get<Array>(value_);
}

const Json::Object& Json::as_object() const {
    if (!is_object()) {
        static const Object empty;
        return empty;
    }
    return std::get<Object>(value_);
}

Json::Array& Json::array_items() {
    if (!is_array()) {
        value_ = Array{};
    }
    return std::get<Array>(value_);
}

Json::Object& Json::object_items() {
    if (!is_object()) {
        value_ = Object{};
    }
    return std::get<Object>(value_);
}

bool Json::contains(std::string_view key) const {
    if (!is_object()) {
        return false;
    }
    const auto& obj = as_object();
    return obj.find(std::string(key)) != obj.end();
}

const Json& Json::at(std::string_view key) const {
    if (!is_object()) {
        return null_json();
    }
    const auto& obj = as_object();
    const auto it = obj.find(std::string(key));
    return it == obj.end() ? null_json() : it->second;
}

const Json& Json::at(std::size_t index) const {
    if (!is_array() || index >= as_array().size()) {
        return null_json();
    }
    return as_array()[index];
}

Json& Json::operator[](std::string key) {
    return object_items()[std::move(key)];
}

Json& Json::operator[](std::size_t index) {
    auto& arr = array_items();
    if (index >= arr.size()) {
        arr.resize(index + 1);
    }
    return arr[index];
}

void Json::push_back(Json value) {
    array_items().push_back(std::move(value));
}

std::string Json::dump(int indent) const {
    std::ostringstream out;
    dump_impl(*this, out, indent, 0);
    return out.str();
}

std::string json_escape(std::string_view value) {
    std::string out;
    for (const char c : value) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    out += "?";
                } else {
                    out.push_back(c);
                }
        }
    }
    return out;
}

}  // namespace oop
