#pragma once

#include "agent/skill_loader.h"
#include "client/llm_client.h"
#include "environment/environment.h"
#include "harness/evaluator.h"
#include "http/http_client.h"

#include <filesystem>
#include <memory>

namespace oop {

struct HarnessConfig {
    std::filesystem::path output_dir = "out";
    std::filesystem::path workspace_dir = "workspace";
    ChatOptions chat_options;
    bool prefer_task_script = true;
    bool clean_task_workspace = true;
};

struct TaskRunSummary {
    std::string id;
    std::string difficulty;
    bool success = false;
    double score = 0.0;
    int step_count = 0;
    int tool_call_count = 0;
    std::string evaluator;
    std::string message;
    Json evaluation_details = Json::object();
    std::filesystem::path trajectory_path;
};

class HarnessRunner {
public:
    HarnessRunner(
        LLMClient& llm,
        SkillLoader& skills,
        HttpClient& http_client,
        HarnessConfig config);

    Result<Json> run_batch(const std::filesystem::path& tasks_file);

private:
    Result<TaskRunSummary> run_one(const Json& task);

    LLMClient& llm_;
    SkillLoader& skills_;
    HttpClient& http_client_;
    HarnessConfig config_;
};

}  // namespace oop
