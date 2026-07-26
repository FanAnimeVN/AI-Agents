#pragma once

#include "core/json.h"
#include "core/result.h"

#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace oop {

struct ActionRecord {
    std::string type = "none";
    std::string tool;
    Json args = Json::object();
};

struct StepRecord {
    int step_id = 0;
    std::string thought;
    ActionRecord action;
    std::string tool_result;
    int tokens_used = 0;
    long long latency_ms = 0;
};

struct Trajectory {
    std::string task_id;
    std::string model;
    bool success = false;
    int total_tokens = 0;
    long long total_time_ms = 0;
    std::vector<StepRecord> steps;
    std::string final_answer;

    [[nodiscard]] Json to_json() const;
    Result<void> save(const std::filesystem::path& path) const;
};

}  // namespace oop
