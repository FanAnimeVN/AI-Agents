#include "tools/file_tools.h"

#include <fstream>
#include <sstream>
#include <system_error>

namespace oop {

std::string ReadFileTool::name() const {
    return "read_file";
}

std::string ReadFileTool::description() const {
    return "Read a UTF-8 text file inside the workspace. Args: {\"path\":\"relative/path.txt\"}.";
}

ToolResult ReadFileTool::execute(const Json& args, ToolExecutionContext& context) {
    const std::string path = args.is_string() ? args.as_string() : args.at("path").as_string_or();
    auto resolved = context.environment.resolve_inside_workspace(path);
    if (!resolved) {
        return ToolResult{false, {}, resolved.error(), Json::object()};
    }
    std::ifstream in(*resolved);
    if (!in) {
        return ToolResult{false, {}, "Cannot open file: " + path, Json::object()};
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    Json meta = Json::object();
    meta["path"] = path;
    return ToolResult{true, buffer.str(), {}, meta};
}

std::string WriteFileTool::name() const {
    return "write_file";
}

std::string WriteFileTool::description() const {
    return "Write a UTF-8 text file inside the workspace. Args: {\"path\":\"file.txt\",\"content\":\"...\",\"append\":false}.";
}

ToolResult WriteFileTool::execute(const Json& args, ToolExecutionContext& context) {
    const std::string path = args.at("path").as_string_or();
    const std::string content = args.at("content").as_string_or();
    const bool append = args.at("append").as_bool(false);
    auto resolved = context.environment.resolve_inside_workspace(path);
    if (!resolved) {
        return ToolResult{false, {}, resolved.error(), Json::object()};
    }
    std::error_code ec;
    std::filesystem::create_directories(resolved->parent_path(), ec);
    if (ec) {
        return ToolResult{false, {}, "Cannot create parent directory: " + ec.message(), Json::object()};
    }
    std::ofstream out(*resolved, append ? std::ios::app : std::ios::trunc);
    if (!out) {
        return ToolResult{false, {}, "Cannot write file: " + path, Json::object()};
    }
    out << content;
    Json meta = Json::object();
    meta["path"] = path;
    meta["bytes"] = static_cast<double>(content.size());
    return ToolResult{true, "Wrote " + std::to_string(content.size()) + " bytes to " + path, {}, meta};
}

}  // namespace oop
