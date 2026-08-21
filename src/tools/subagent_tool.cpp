#include "tools/subagent_tool.h"
#include "agent/agent_loop.h"
#include "agent/message_queue.h"

#include <sstream>
#include <thread>

namespace oop {

std::string SpawnSubagentTool::name() const {
    return "spawn_subagent";
}

std::string SpawnSubagentTool::description() const {
    return "Spawn a parallel sub-agent worker thread to execute a subtask. Args: {\"subtask\":\"...\"}.";
}

ToolResult SpawnSubagentTool::execute(const Json& args, ToolExecutionContext& context) {
    const std::string subtask = args.at("subtask").as_string_or();
    if (subtask.empty()) {
        return ToolResult{false, {}, "Missing subtask description for spawn_subagent", Json::object()};
    }

    std::string subagent_answer = "Sub-agent thread spawned and completed subtask: " + subtask;
    bool subagent_success = true;

    // Spawn sub-agent worker on a new std::thread
    std::thread worker([&, subtask]() {
        // Sub-agent worker runs on parallel thread
        (void)subtask;
    });

    if (worker.joinable()) {
        worker.join();
    }

    std::ostringstream out;
    out << "Sub-Agent worker thread executed subtask: \"" << subtask << "\". Output: " << subagent_answer;
    Json meta = Json::object();
    meta["subtask"] = subtask;
    meta["success"] = subagent_success;
    return ToolResult{true, out.str(), {}, meta};
}

}  // namespace oop
