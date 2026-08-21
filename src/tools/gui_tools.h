#pragma once

#include "tools/tool.h"

namespace oop {

class ClickTool final : public Tool {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    ToolResult execute(const Json& args, ToolExecutionContext& context) override;
};

class TypeTextTool final : public Tool {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    ToolResult execute(const Json& args, ToolExecutionContext& context) override;
};

class KeyPressTool final : public Tool {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    ToolResult execute(const Json& args, ToolExecutionContext& context) override;
};

class GuiBrowserSearchTool final : public Tool {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    ToolResult execute(const Json& args, ToolExecutionContext& context) override;
};

}  // namespace oop
