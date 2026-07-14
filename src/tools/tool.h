#pragma once

#include "core/json.h"
#include "core/result.h"
#include "environment/environment.h"
#include "http/http_client.h"

#include <concepts>
#include <functional>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace oop {

struct ToolExecutionContext {
    Environment& environment;
    HttpClient& http_client;
};

struct ToolResult {
    bool success = false;
    std::string output;
    std::string error;
    Json metadata = Json::object();

    [[nodiscard]] std::string observation_text() const {
        if (success) {
            return output;
        }
        return "ERROR: " + error;
    }
};

class Tool {
public:
    virtual ~Tool() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    [[nodiscard]] virtual std::string description() const = 0;
    virtual ToolResult execute(const Json& args, ToolExecutionContext& context) = 0;
};

template <class T>
concept ToolLike = std::derived_from<T, Tool>;

struct ToolPolicy {
    std::set<std::string> allow;
    std::set<std::string> deny;

    [[nodiscard]] bool is_allowed(const std::string& tool_name) const {
        if (deny.contains(tool_name)) {
            return false;
        }
        return allow.empty() || allow.contains(tool_name);
    }
};

using ToolFactory = std::function<std::unique_ptr<Tool>()>;

}  // namespace oop
