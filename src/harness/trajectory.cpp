#include "harness/trajectory.h"

#include <fstream>
#include <system_error>

namespace oop {

Json Trajectory::to_json() const {
    Json root = Json::object();
    root["task_id"] = task_id;
    root["model"] = model;
    root["success"] = success;
    root["total_tokens"] = total_tokens;
    root["total_time_ms"] = static_cast<double>(total_time_ms);
    root["final_answer"] = final_answer;
    Json steps_json = Json::array();
    for (const auto& step : steps) {
        Json item = Json::object();
        item["step_id"] = step.step_id;
        item["thought"] = step.thought;
        item["tokens_used"] = step.tokens_used;
        item["latency_ms"] = static_cast<double>(step.latency_ms);
        item["tool_result"] = step.tool_result;
        Json action = Json::object();
        action["type"] = step.action.type;
        action["tool"] = step.action.tool;
        action["args"] = step.action.args;
        item["action"] = std::move(action);
        steps_json.push_back(std::move(item));
    }
    root["steps"] = std::move(steps_json);
    return root;
}

Result<void> Trajectory::save(const std::filesystem::path& path) const {
    std::error_code ec;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            return std::unexpected("Cannot create trajectory directory: " + ec.message());
        }
    }
    std::ofstream out(path);
    if (!out) {
        return std::unexpected("Cannot write trajectory: " + path.string());
    }
    out << to_json().dump(2) << '\n';
    return {};
}

}  // namespace oop
