#include "tools/calculator_tool.h"

#include "core/string_utils.h"

#include <charconv>
#include <cmath>
#include <sstream>

namespace oop {
namespace {

class ExprParser {
public:
    explicit ExprParser(std::string_view input) : input_(input) {}

    Result<double> parse() {
        auto value = expression();
        if (!value) {
            return value;
        }
        skip_ws();
        if (pos_ != input_.size()) {
            return fail<double>("Unexpected token near: " + std::string(input_.substr(pos_)));
        }
        return value;
    }

private:
    Result<double> expression() {
        auto lhs = term();
        if (!lhs) {
            return lhs;
        }
        while (true) {
            skip_ws();
            if (match('+')) {
                auto rhs = term();
                if (!rhs) {
                    return rhs;
                }
                *lhs += *rhs;
            } else if (match('-')) {
                auto rhs = term();
                if (!rhs) {
                    return rhs;
                }
                *lhs -= *rhs;
            } else {
                return lhs;
            }
        }
    }

    Result<double> term() {
        auto lhs = factor();
        if (!lhs) {
            return lhs;
        }
        while (true) {
            skip_ws();
            if (match('*')) {
                auto rhs = factor();
                if (!rhs) {
                    return rhs;
                }
                *lhs *= *rhs;
            } else if (match('/')) {
                auto rhs = factor();
                if (!rhs) {
                    return rhs;
                }
                if (*rhs == 0.0) {
                    return fail<double>("Division by zero");
                }
                *lhs /= *rhs;
            } else {
                return lhs;
            }
        }
    }

    Result<double> factor() {
        skip_ws();
        if (match('-')) {
            auto inner = factor();
            if (!inner) {
                return inner;
            }
            return -*inner;
        }
        if (match('(')) {
            auto value = expression();
            if (!value) {
                return value;
            }
            if (!match(')')) {
                return fail<double>("Expected closing parenthesis");
            }
            return value;
        }
        return number();
    }

    Result<double> number() {
        skip_ws();
        const std::size_t begin = pos_;
        while (pos_ < input_.size() &&
               (std::isdigit(static_cast<unsigned char>(input_[pos_])) || input_[pos_] == '.')) {
            ++pos_;
        }
        if (begin == pos_) {
            return fail<double>("Expected number");
        }
        double value = 0.0;
        const auto token = input_.substr(begin, pos_ - begin);
        const auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), value);
        if (ec != std::errc{}) {
            return fail<double>("Invalid number: " + std::string(token));
        }
        return value;
    }

    bool match(char c) {
        skip_ws();
        if (pos_ < input_.size() && input_[pos_] == c) {
            ++pos_;
            return true;
        }
        return false;
    }

    void skip_ws() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
    }

    std::string_view input_;
    std::size_t pos_ = 0;
};

}  // namespace

std::string CalculatorTool::name() const {
    return "calculator";
}

std::string CalculatorTool::description() const {
    return "Evaluate arithmetic expressions with +, -, *, / and parentheses. Args: {\"expression\":\"15*17\"}.";
}

ToolResult CalculatorTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)context;
    const std::string expression = args.is_string() ? args.as_string() : args.at("expression").as_string_or();
    auto value = ExprParser(expression).parse();
    if (!value) {
        return ToolResult{false, {}, value.error(), Json::object()};
    }
    std::ostringstream out;
    if (std::floor(*value) == *value) {
        out << static_cast<long long>(*value);
    } else {
        out << *value;
    }
    Json meta = Json::object();
    meta["expression"] = expression;
    meta["value"] = *value;
    return ToolResult{true, out.str(), {}, meta};
}

}  // namespace oop
