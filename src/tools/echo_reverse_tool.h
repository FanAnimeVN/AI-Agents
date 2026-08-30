#pragma once
#include "tools/tool.h"
#include <algorithm>

namespace oop {

class EchoReverseTool final : public Tool {
public:
    std::string name() const override { return "echo_reverse"; }
    std::string description() const override { return "Reverse an input text string. Args: {\"text\":\"...\"}."; }

    ToolResult execute(const Json& args, ToolExecutionContext& context) override {
        (void)context;
        std::string text = args.at("text").as_string_or();
        std::reverse(text.begin(), text.end());
        return ToolResult{true, text, {}, Json::object()};
    }
};

}  // namespace oop