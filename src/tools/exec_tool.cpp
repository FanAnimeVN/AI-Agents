#include "tools/exec_tool.h"

#include <array>
#include <cstdio>
#include <sstream>

namespace oop {
namespace {

Result<std::string> run_command(const std::string& command) {
#ifdef _WIN32
    const std::string shell_command = "cmd /C \"" + command + " 2>&1\"";
    FILE* pipe = _popen(shell_command.c_str(), "r");
#else
    const std::string shell_command = command + " 2>&1";
    FILE* pipe = popen(shell_command.c_str(), "r");
#endif
    if (!pipe) {
        return fail<std::string>("Cannot start shell command");
    }
    std::array<char, 4096> buffer{};
    std::string output;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
#ifdef _WIN32
    const int code = _pclose(pipe);
#else
    const int code = pclose(pipe);
#endif
    if (code != 0) {
        output += "\n(exit_code=" + std::to_string(code) + ")";
    }
    return output;
}

}  // namespace

std::string ExecTool::name() const {
    return "exec";
}

std::string ExecTool::description() const {
    return "Run a shell command and return stdout/stderr. Args: {\"command\":\"...\"}. Use only for safe commands.";
}

ToolResult ExecTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)context;
    const std::string command = args.is_string() ? args.as_string() : args.at("command").as_string_or();
    if (command.empty()) {
        return ToolResult{false, {}, "Missing command", Json::object()};
    }
    auto output = run_command(command);
    if (!output) {
        return ToolResult{false, {}, output.error(), Json::object()};
    }
    Json meta = Json::object();
    meta["command"] = command;
    return ToolResult{true, *output, {}, meta};
}

}  // namespace oop
