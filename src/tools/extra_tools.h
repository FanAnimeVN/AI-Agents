#pragma once

#include "tools/tool.h"

namespace oop {

class TimeTool final : public Tool {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    ToolResult execute(const Json& args, ToolExecutionContext& context) override;
};

class TextStatsTool final : public Tool {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    ToolResult execute(const Json& args, ToolExecutionContext& context) override;
};

class EnvironmentInfoTool final : public Tool {
public:
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    ToolResult execute(const Json& args, ToolExecutionContext& context) override;
};

}  // namespace oop
