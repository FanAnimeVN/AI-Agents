#pragma once

#include "agent/loop_detector.h"
#include "agent/skill_loader.h"
#include "client/llm_client.h"
#include "harness/trajectory.h"
#include "tools/tool_registry.h"

#include <functional>
#include <memory>
#include <optional>
#include <variant>

namespace oop {

struct ToolCallAction {
    std::string tool;
    Json args = Json::object();
};

struct DoneAction {
    std::string answer;
};

struct ClickAction {
    int x = 0;
    int y = 0;
};

struct TypeTextAction {
    std::string text;
};

struct KeyPressAction {
    std::string key;
};

using AgentAction = std::variant<ToolCallAction, DoneAction, ClickAction, TypeTextAction, KeyPressAction>;

struct AgentRunConfig {
    int max_steps = 8;
    ChatOptions chat_options;
    std::string task_id = "manual";
    // Base64 image payloads are attached to the initial user message. Keeping
    // them in the run configuration preserves one AgentLoop interface for
    // text-only and multimodal Ollama runs.
    std::vector<std::string> images_base64;
};

struct AgentRunResult {
    bool success = false;
    std::string final_answer;
    std::string stop_reason;
    Trajectory trajectory;
};

using StepHook = std::function<void(const StepRecord&)>;

class AgentLoop {
public:
    virtual ~AgentLoop() = default;

    AgentLoop(
        LLMClient& llm,
        ToolRegistry& tools,
        SkillLoader& skills,
        ToolExecutionContext& tool_context,
        LoopDetector detector = LoopDetector{});

    void set_step_hook(StepHook hook);

    // Template Method: run() owns the ReAct skeleton; subclasses can override
    // observe(), build_system_prompt(), parse_action(), and after_step().
    virtual AgentRunResult run(const std::string& task, const AgentRunConfig& config);

protected:
    virtual std::string build_system_prompt(const std::string& task);
    virtual std::string observe(const ToolResult& tool_result);
    virtual Result<AgentAction> parse_action(const std::string& llm_text) const;
    virtual void after_step(const StepRecord& step);

private:
    LLMClient& llm_;
    ToolRegistry& tools_;
    SkillLoader& skills_;
    ToolExecutionContext& tool_context_;
    LoopDetector detector_;
    StepHook hook_;
};

std::string action_signature(const AgentAction& action);

}  // namespace oop
