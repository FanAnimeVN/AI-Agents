#pragma once

#include "tools/tool.h"

#include <map>
#include <optional>

namespace oop {

template <class Base>
requires std::derived_from<Base, Tool>
class Registry {
public:
    using Factory = std::function<std::unique_ptr<Base>()>;

    void register_factory(std::string name, Factory factory) {
        factories_[std::move(name)] = std::move(factory);
    }

    [[nodiscard]] bool contains(const std::string& name) const {
        return factories_.contains(name);
    }

    [[nodiscard]] std::unique_ptr<Base> create(const std::string& name) const {
        const auto it = factories_.find(name);
        if (it == factories_.end()) {
            return nullptr;
        }
        return it->second();
    }

    [[nodiscard]] std::vector<std::string> names() const {
        std::vector<std::string> out;
        for (const auto& [name, _] : factories_) {
            out.push_back(name);
        }
        return out;
    }

private:
    std::map<std::string, Factory> factories_;
};

class ToolRegistry {
public:
    void register_tool(std::unique_ptr<Tool> tool);
    void register_factory(std::string name, ToolFactory factory);
    [[nodiscard]] bool has_tool(const std::string& name) const;
    [[nodiscard]] std::vector<std::string> tool_names() const;
    [[nodiscard]] std::string tools_prompt() const;
    ToolResult execute(const std::string& name, const Json& args, ToolExecutionContext& context) const;

    ToolPolicy& policy();
    const ToolPolicy& policy() const;

private:
    std::map<std::string, std::unique_ptr<Tool>> tools_;
    Registry<Tool> factories_;
    ToolPolicy policy_;
};

std::unique_ptr<ToolRegistry> make_default_tool_registry();

}  // namespace oop
