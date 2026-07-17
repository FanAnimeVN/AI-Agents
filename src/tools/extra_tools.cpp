#include "tools/extra_tools.h"

#include "core/string_utils.h"

#include <chrono>
#include <cstdlib>
#include <generator>
#include <iomanip>
#include <set>
#include <sstream>

namespace oop {
namespace {

std::generator<std::string> words(std::string_view text) {
    for (auto word : str::split_words(text)) {
        co_yield word;
    }
}

}  // namespace

std::string TimeTool::name() const {
    return "time";
}

std::string TimeTool::description() const {
    return "Return current local time in ISO-like format. Args: {}.";
}

ToolResult TimeTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)args;
    (void)context;
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return ToolResult{true, out.str(), {}, Json::object()};
}

std::string TextStatsTool::name() const {
    return "text_stats";
}

std::string TextStatsTool::description() const {
    return "Count characters, words and unique normalized words. Args: {\"text\":\"...\"}.";
}

ToolResult TextStatsTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)context;
    const std::string text = args.at("text").as_string_or();
    std::set<std::string> unique;
    int count = 0;
    for (const auto& word : words(text)) {
        unique.insert(word);
        ++count;
    }
    Json meta = Json::object();
    meta["characters"] = static_cast<double>(text.size());
    meta["words"] = count;
    meta["unique_words"] = static_cast<double>(unique.size());
    std::ostringstream out;
    out << "characters=" << text.size() << ", words=" << count << ", unique_words=" << unique.size();
    return ToolResult{true, out.str(), {}, meta};
}

std::string EnvironmentInfoTool::name() const {
    return "env_info";
}

std::string EnvironmentInfoTool::description() const {
    return "Return agent workspace and selected environment variables. Args: {}.";
}

ToolResult EnvironmentInfoTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)args;
    Json meta = Json::object();
    meta["workspace"] = context.environment.workspace_root().string();
    const char* model = std::getenv("OOP_AGENT_MODEL");
    meta["OOP_AGENT_MODEL"] = model == nullptr ? "" : model;
    return ToolResult{true, meta.dump(2), {}, meta};
}

}  // namespace oop
