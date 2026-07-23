#include "agent/agent_loop.h"

#include "core/string_utils.h"

#include <chrono>
#include <sstream>

namespace oop {
namespace {

std::string strip_prefix(std::string text, std::string_view prefix) {
    text = str::trim(text);
    if (text.starts_with(prefix)) {
        return str::trim(std::string_view(text).substr(prefix.size()));
    }
    return text;
}

}  // namespace

AgentLoop::AgentLoop(
    LLMClient& llm,
    ToolRegistry& tools,
    SkillLoader& skills,
    ToolExecutionContext& tool_context,
    LoopDetector detector)
    : llm_(llm), tools_(tools), skills_(skills), tool_context_(tool_context), detector_(std::move(detector)) {}

void AgentLoop::set_step_hook(StepHook hook) {
    hook_ = std::move(hook);
}

AgentRunResult AgentLoop::run(const std::string& task, const AgentRunConfig& config) {
    // A detector belongs to one AgentLoop object, but its history belongs to
    // one run. Reusing the same loop must not make a new task inherit old
    // action signatures.
    detector_.reset();

    AgentRunResult result;
    result.trajectory.task_id = config.task_id;
    result.trajectory.model = config.chat_options.model;
    result.trajectory.success = false;

    const auto start = std::chrono::steady_clock::now();
    std::vector<ChatMessage> messages{
        {"system", build_system_prompt(task), {}},
        {"user", task, config.images_base64}};

    for (int step_id = 0; step_id < config.max_steps; ++step_id) {
        auto response = llm_.chat(ChatRequest{messages, config.chat_options});
        StepRecord step;
        step.step_id = step_id;
        if (!response) {
            result.stop_reason = response.error();
            step.tool_result = "LLM error: " + response.error();
            result.trajectory.steps.push_back(step);
            after_step(step);
            break;
        }

        step.thought = response->content;
        step.tokens_used = response->tokens_used;
        step.latency_ms = response->latency_ms;
        result.trajectory.total_tokens += response->tokens_used;
        messages.push_back({"assistant", response->content, {}});

        auto action = parse_action(response->content);
        if (!action) {
            step.action = {"parse_error", {}, Json::object()};
            step.tool_result = action.error();
            messages.push_back({"user", "OBSERVATION: " + action.error(), {}});
            result.trajectory.steps.push_back(step);
            after_step(step);
            continue;
        }

        const auto signature = action_signature(*action);
        const auto loop = detector_.observe_action(signature);
        std::string loop_notice;
        if (loop.severity == LoopSeverity::Warning) {
            loop_notice = "LOOP WARNING: " + loop.reason;
        } else if (loop.severity == LoopSeverity::Critical) {
            loop_notice = "LOOP CRITICAL: " + loop.reason;
        }

        const auto add_loop_notice = [&loop_notice](std::string observation) {
            if (loop_notice.empty()) {
                return observation;
            }
            return loop_notice + '\n' + observation;
        };

        std::visit([&](const auto& concrete) {
            using T = std::decay_t<decltype(concrete)>;
            if constexpr (std::is_same_v<T, ToolCallAction>) {
                step.action = {"tool_call", concrete.tool, concrete.args};
                if (loop.severity == LoopSeverity::Critical) {
                    result.stop_reason = loop.reason;
                    step.tool_result = loop_notice;
                    return;
                }
                auto tool_result = tools_.execute(concrete.tool, concrete.args, tool_context_);
                step.tool_result = add_loop_notice(observe(tool_result));
                messages.push_back({"user", "OBSERVATION: " + step.tool_result, {}});
            } else if constexpr (std::is_same_v<T, DoneAction>) {
                step.action = {"done", {}, Json::object()};
                step.tool_result = add_loop_notice("Done");
                result.success = true;
                result.final_answer = concrete.answer;
                result.stop_reason = "final";
            } else if constexpr (std::is_same_v<T, ClickAction>) {
                step.action = {"click", "capture_screenshot", Json::object()};
                step.tool_result = add_loop_notice(
                    "GUI action preview: click(" + std::to_string(concrete.x) + "," +
                    std::to_string(concrete.y) + ")");
                messages.push_back({"user", "OBSERVATION: " + step.tool_result, {}});
            } else if constexpr (std::is_same_v<T, TypeTextAction>) {
                step.action = {"type_text", "gui", Json::object()};
                step.tool_result = add_loop_notice("GUI action preview: type_text(" + concrete.text + ")");
                messages.push_back({"user", "OBSERVATION: " + step.tool_result, {}});
            } else if constexpr (std::is_same_v<T, KeyPressAction>) {
                step.action = {"key_press", "gui", Json::object()};
                step.tool_result = add_loop_notice("GUI action preview: key_press(" + concrete.key + ")");
                messages.push_back({"user", "OBSERVATION: " + step.tool_result, {}});
            }
        }, *action);

        result.trajectory.steps.push_back(step);
        after_step(step);
        if (result.success || !result.stop_reason.empty()) {
            break;
        }
    }

    if (!result.success && result.stop_reason.empty()) {
        result.stop_reason = "max_steps reached";
    }
    result.trajectory.success = result.success;
    result.trajectory.final_answer = result.final_answer;
    const auto end = std::chrono::steady_clock::now();
    result.trajectory.total_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    return result;
}

std::string AgentLoop::build_system_prompt(const std::string& task) {
    const auto selected = skills_.select_for_task(task);
    std::ostringstream out;
    out << "You are a C++ OOP project agent using ReAct.\n";
    out << "Format rules (STRICT):\n";
    out << "- Think privately in short text.\n";
    out << "- To use a tool, respond ONLY in exact JSON: ACTION: {\"tool\":\"tool_name\",\"args\":{\"param\":\"value\"}}\n";
    out << "- Never omit \"tool\" or write ACTION: tool_name{...}. Always use valid JSON.\n";
    out << "- Complete ALL requested steps in order before issuing FINAL.\n";
    out << "- Include all requested words, numbers, and status labels (e.g. PASS, FAIL, verified) in your output.\n";
    out << "- Finish with FINAL: <answer>\n\n";
    out << tools_.tools_prompt() << "\n\n";
    out << skills_.build_prompt(selected);
    return out.str();
}

std::string AgentLoop::observe(const ToolResult& tool_result) {
    return tool_result.observation_text();
}

Result<AgentAction> AgentLoop::parse_action(const std::string& llm_text) const {
    const auto action_pos = llm_text.find("ACTION:");
    const auto final_pos = llm_text.find("FINAL:");

    if (action_pos != std::string::npos && (final_pos == std::string::npos || action_pos < final_pos)) {
        std::string raw = llm_text.substr(action_pos);
        const auto start_brace = raw.find('{');
        if (start_brace != std::string::npos) {
            std::string prefix_tool = str::trim(raw.substr(7, start_brace - 7));
            int depth = 0;
            std::size_t end_brace = std::string::npos;
            for (std::size_t i = start_brace; i < raw.size(); ++i) {
                if (raw[i] == '{') {
                    depth++;
                } else if (raw[i] == '}') {
                    depth--;
                    if (depth == 0) {
                        end_brace = i;
                        break;
                    }
                }
            }
            if (end_brace != std::string::npos) {
                std::string json_str = raw.substr(start_brace, end_brace - start_brace + 1);
                
                // Fix unquoted keys like query: -> "query":
                std::string fixed_json;
                fixed_json.reserve(json_str.size() + 16);
                for (std::size_t i = 0; i < json_str.size(); ++i) {
                    if (std::isalpha(static_cast<unsigned char>(json_str[i])) && 
                        (i == 0 || json_str[i-1] == '{' || json_str[i-1] == ',' || std::isspace(static_cast<unsigned char>(json_str[i-1])))) {
                        std::size_t colon_pos = json_str.find(':', i);
                        if (colon_pos != std::string::npos && json_str.find('"', i) > colon_pos) {
                            std::string key = str::trim(json_str.substr(i, colon_pos - i));
                            fixed_json += "\"" + key + "\":";
                            i = colon_pos;
                            continue;
                        }
                    }
                    fixed_json.push_back(json_str[i]);
                }

                auto parsed = Json::parse(fixed_json);
                if (!parsed) {
                    parsed = Json::parse(json_str);
                }

                if (parsed) {
                    std::string tool = parsed->at("tool").as_string_or();
                    if (tool.empty() && !prefix_tool.empty()) {
                        tool = prefix_tool;
                    }
                    Json args = parsed->contains("args") ? parsed->at("args") : *parsed;
                    if (!tool.empty()) {
                        return ToolCallAction{tool, args};
                    }
                } else if (!prefix_tool.empty()) {
                    // Fallback: extract string inside quotes if args parsing failed
                    Json fallback_args = Json::object();
                    const auto q1 = json_str.find('"');
                    const auto q2 = json_str.rfind('"');
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                        fallback_args["query"] = json_str.substr(q1 + 1, q2 - q1 - 1);
                        fallback_args["expression"] = json_str.substr(q1 + 1, q2 - q1 - 1);
                    }
                    return ToolCallAction{prefix_tool, fallback_args};
                }
            }
        }
    }

    if (final_pos != std::string::npos) {
        return DoneAction{strip_prefix(llm_text.substr(final_pos), "FINAL:")};
    }

    return fail<AgentAction>("No valid ACTION or FINAL marker found");
}

void AgentLoop::after_step(const StepRecord& step) {
    if (hook_) {
        hook_(step);
    }
}

std::string action_signature(const AgentAction& action) {
    return std::visit([](const auto& concrete) -> std::string {
        using T = std::decay_t<decltype(concrete)>;
        if constexpr (std::is_same_v<T, ToolCallAction>) {
            return "tool:" + concrete.tool + ":" + concrete.args.dump();
        } else if constexpr (std::is_same_v<T, DoneAction>) {
            return "done";
        } else if constexpr (std::is_same_v<T, ClickAction>) {
            return "click:" + std::to_string(concrete.x) + "," + std::to_string(concrete.y);
        } else if constexpr (std::is_same_v<T, TypeTextAction>) {
            return "type:" + concrete.text;
        } else {
            return "key:" + concrete.key;
        }
    }, action);
}

}  // namespace oop
