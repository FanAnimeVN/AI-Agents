#include "tools/tool_registry.h"

#include "tools/calculator_tool.h"
#include "tools/exec_tool.h"
#include "tools/extra_tools.h"
#include "tools/file_tools.h"
#include "tools/gui_tools.h"
#include "tools/memory_tools.h"
#include "tools/screenshot_tool.h"
#include "tools/vector_memory_tools.h"
#include "tools/web_search_tool.h"
#include "tools/echo_reverse_tool.h"

#include <algorithm>
#include <sstream>

#include "tools/subagent_tool.h"

namespace oop {

void ToolRegistry::register_tool(std::unique_ptr<Tool> tool) {
    if (!tool) {
        return;
    }
    const auto key = tool->name();
    tools_[key] = std::move(tool);
}

void ToolRegistry::register_factory(std::string name, ToolFactory factory) {
    factories_.register_factory(std::move(name), std::move(factory));
}

bool ToolRegistry::has_tool(const std::string& name) const {
    return tools_.contains(name) || factories_.contains(name);
}

std::vector<std::string> ToolRegistry::tool_names() const {
    std::vector<std::string> names;
    for (const auto& [name, _] : tools_) {
        names.push_back(name);
    }
    for (const auto& name : factories_.names()) {
        if (!std::ranges::contains(names, name)) {
            names.push_back(name);
        }
    }
    return names;
}

std::string ToolRegistry::tools_prompt() const {
    std::ostringstream out;
    out << "Available tools. To call one, respond exactly as ACTION: {\"tool\":\"name\",\"args\":{...}}\n";
    for (const auto& [name, tool] : tools_) {
        out << "- " << name << ": " << tool->description() << '\n';
    }
    for (const auto& name : factories_.names()) {
        if (!tools_.contains(name)) {
            auto tool = factories_.create(name);
            out << "- " << name << ": " << tool->description() << '\n';
        }
    }
    out << "When finished, respond as FINAL: <answer>.";
    return out.str();
}

ToolResult ToolRegistry::execute(const std::string& name, const Json& args, ToolExecutionContext& context) const {
    if (!policy_.is_allowed(name)) {
        return ToolResult{false, {}, "Tool denied by policy: " + name, Json::object()};
    }
    if (const auto it = tools_.find(name); it != tools_.end()) {
        return it->second->execute(args, context);
    }
    auto tool = factories_.create(name);
    if (tool) {
        return tool->execute(args, context);
    }
    return ToolResult{false, {}, "Unknown tool: " + name, Json::object()};
}

ToolPolicy& ToolRegistry::policy() {
    return policy_;
}

const ToolPolicy& ToolRegistry::policy() const {
    return policy_;
}

std::unique_ptr<ToolRegistry> make_default_tool_registry() {
    auto registry = std::make_unique<ToolRegistry>();
    registry->register_tool(std::make_unique<CalculatorTool>());
    registry->register_tool(std::make_unique<ExecTool>());
    registry->register_tool(std::make_unique<ReadFileTool>());
    registry->register_tool(std::make_unique<WriteFileTool>());
    registry->register_tool(std::make_unique<WebSearchTool>());
    registry->register_tool(std::make_unique<MemorySaveTool>());
    registry->register_tool(std::make_unique<MemorySearchTool>());
    registry->register_tool(std::make_unique<VectorMemorySaveTool>());
    registry->register_tool(std::make_unique<VectorMemorySearchTool>());
    registry->register_tool(std::make_unique<ScreenshotTool>());
    registry->register_tool(std::make_unique<ClickTool>());
    registry->register_tool(std::make_unique<TypeTextTool>());
    registry->register_tool(std::make_unique<KeyPressTool>());
    registry->register_tool(std::make_unique<GuiBrowserSearchTool>());
    registry->register_tool(std::make_unique<SpawnSubagentTool>());
    registry->register_tool(std::make_unique<TextStatsTool>());
    registry->register_tool(std::make_unique<EnvironmentInfoTool>());
    registry->register_tool(std::make_unique<EchoReverseTool>());
    // Register one default tool lazily to demonstrate the Factory path.
    registry->register_factory("time", [] {
        return std::make_unique<TimeTool>();
    });
    return registry;
}

}  // namespace oop
