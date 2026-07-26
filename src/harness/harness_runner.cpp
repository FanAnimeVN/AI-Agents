#include "harness/harness_runner.h"

#include "agent/agent_loop.h"
#include "client/scripted_llm_client.h"
#include "tools/tool_registry.h"

#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string_view>
#include <system_error>

namespace oop {
namespace {

constexpr std::string_view kScriptedModelLabel = "scripted:task-fixture";

Result<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return fail<std::string>("Cannot open task file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<std::string> scripted_responses_from_task(const Json& task) {
    std::vector<std::string> responses;
    for (const auto& item : task.at("scripted_responses").as_array()) {
        responses.push_back(item.as_string_or());
    }
    return responses;
}

Result<void> seed_initial_files(const Json& task, const Environment& environment) {
    if (!task.contains("initial_files")) {
        return {};
    }

    for (const auto& file : task.at("initial_files").as_array()) {
        const auto path = file.at("path").as_string_or();
        auto resolved = environment.resolve_inside_workspace(path);
        if (!resolved) {
            return std::unexpected("Invalid initial file path " + path + ": " + resolved.error());
        }

        std::error_code ec;
        std::filesystem::create_directories(resolved->parent_path(), ec);
        if (ec) {
            return std::unexpected("Cannot create initial file directory: " + ec.message());
        }

        std::ofstream out(*resolved, std::ios::trunc);
        if (!out) {
            return std::unexpected("Cannot write initial file: " + path);
        }
        out << file.at("content").as_string_or();
    }
    return {};
}

Result<void> validate_task_suite(const Json& tasks) {
    if (!tasks.is_array()) {
        return std::unexpected("Task file must be a JSON array");
    }
    if (tasks.as_array().size() < 10) {
        return std::unexpected("Benchmark requires at least 10 tasks");
    }

    std::set<std::string> ids;
    std::map<std::string, int> difficulty_counts;
    for (const auto& task : tasks.as_array()) {
        if (!task.is_object()) {
            return std::unexpected("Each benchmark task must be a JSON object");
        }
        const auto id = task.at("id").as_string_or();
        if (id.empty() || !ids.insert(id).second) {
            return std::unexpected("Task ids must be present and unique");
        }
        const std::filesystem::path id_path(id);
        if (id == "." || id == ".." || id_path.has_parent_path() || id_path.is_absolute()) {
            return std::unexpected("Task " + id + " must use a single safe path component as id");
        }
        const auto difficulty = task.at("difficulty").as_string_or();
        if (difficulty != "easy" && difficulty != "medium" && difficulty != "hard") {
            return std::unexpected("Task " + id + " has invalid difficulty");
        }
        ++difficulty_counts[difficulty];

        if (task.at("instruction").as_string_or().empty()) {
            return std::unexpected("Task " + id + " has no instruction");
        }
        const auto eval_type = task.at("eval_type").as_string_or();
        if (eval_type != "functional" && eval_type != "keyword" && eval_type != "vlm") {
            return std::unexpected("Task " + id + " has invalid eval_type");
        }
        if (!task.contains("max_steps") || task.at("max_steps").as_number(0) <= 0) {
            return std::unexpected("Task " + id + " must have positive max_steps");
        }
        if (!task.contains("scripted_responses") || !task.at("scripted_responses").is_array() ||
            task.at("scripted_responses").as_array().empty()) {
            return std::unexpected("Task " + id + " must have scripted_responses for reproducible offline evaluation");
        }
        if (task.at("max_steps").as_number() < static_cast<double>(task.at("scripted_responses").as_array().size())) {
            return std::unexpected("Task " + id + " max_steps is smaller than its scripted response sequence");
        }
        if (task.contains("min_steps") &&
            task.at("min_steps").as_number() > static_cast<double>(task.at("scripted_responses").as_array().size())) {
            return std::unexpected("Task " + id + " scripted response sequence is shorter than min_steps");
        }

        if (task.contains("initial_files")) {
            if (!task.at("initial_files").is_array()) {
                return std::unexpected("Task " + id + " initial_files must be an array");
            }
            for (const auto& file : task.at("initial_files").as_array()) {
                if (!file.is_object() || file.at("path").as_string_or().empty() || !file.contains("content")) {
                    return std::unexpected("Task " + id + " has an invalid initial_files entry");
                }
            }
        }

        if (task.contains("expected_tool_events")) {
            if (!task.at("expected_tool_events").is_array()) {
                return std::unexpected("Task " + id + " expected_tool_events must be an array");
            }
            for (const auto& event : task.at("expected_tool_events").as_array()) {
                if (!event.is_object() || event.at("tool").as_string_or().empty()) {
                    return std::unexpected("Task " + id + " has an invalid expected_tool_events entry");
                }
            }
        }

        if (task.contains("ordered_event_pairs") && !task.at("ordered_event_pairs").is_array()) {
            return std::unexpected("Task " + id + " ordered_event_pairs must be an array");
        }

        if (task.contains("expected_tool_event_order")) {
            const auto order_mode = task.at("expected_tool_event_order").as_string_or();
            if (order_mode != "sequential" && order_mode != "any") {
                return std::unexpected("Task " + id + " has invalid expected_tool_event_order");
            }
        }

        if (task.contains("max_tool_calls") && task.at("max_tool_calls").as_number(0) < 0) {
            return std::unexpected("Task " + id + " max_tool_calls must be non-negative");
        }
        if (task.contains("min_tool_calls") && task.contains("max_tool_calls") &&
            task.at("min_tool_calls").as_number(0) > task.at("max_tool_calls").as_number(0)) {
            return std::unexpected("Task " + id + " has min_tool_calls greater than max_tool_calls");
        }

        if (difficulty == "medium" || difficulty == "hard") {
            const auto minimum_tool_calls = task.at("min_tool_calls").as_number(0);
            if (!task.contains("min_tool_calls") || minimum_tool_calls < 3) {
                return std::unexpected("Medium/hard task " + id + " must declare at least 3 required tool calls");
            }
            const bool has_order = task.contains("required_tool_order") && task.at("required_tool_order").is_array() &&
                                   task.at("required_tool_order").as_array().size() >= 2;
            const bool has_tool_set = task.contains("required_tools") && task.at("required_tools").is_array() &&
                                      task.at("required_tools").as_array().size() >= 2;
            if (!has_order && !has_tool_set) {
                return std::unexpected("Medium/hard task " + id + " must declare required_tool_order or required_tools");
            }
            if ((has_order && task.at("required_tool_order").as_array().size() > static_cast<std::size_t>(minimum_tool_calls)) ||
                !task.contains("min_steps") || task.at("min_steps").as_number(0) < minimum_tool_calls + 1) {
                return std::unexpected("Medium/hard task " + id + " has inconsistent step/tool contracts");
            }
        }
        if (difficulty == "hard") {
            if (task.at("min_tool_calls").as_number(0) < 5 ||
                !task.contains("required_tools") || !task.at("required_tools").is_array() ||
                task.at("required_tools").as_array().size() < 5 ||
                !task.contains("required_action_types") || !task.at("required_action_types").is_array() ||
                task.at("required_action_types").as_array().empty()) {
                return std::unexpected("Hard task " + id + " must require at least 5 tools and an action-type contract");
            }
            if (!task.contains("expected_tool_events") || !task.at("expected_tool_events").is_array() ||
                task.at("expected_tool_events").as_array().size() < 2) {
                return std::unexpected("Hard task " + id + " must verify at least two concrete tool events");
            }
        }
    }

    if (difficulty_counts["easy"] < 4 || difficulty_counts["medium"] < 4 || difficulty_counts["hard"] < 2) {
        return std::unexpected("Benchmark difficulty distribution must include at least 4 easy, 4 medium and 2 hard tasks");
    }
    return {};
}

}  // namespace

HarnessRunner::HarnessRunner(
    LLMClient& llm,
    SkillLoader& skills,
    HttpClient& http_client,
    HarnessConfig config)
    : llm_(llm), skills_(skills), http_client_(http_client), config_(std::move(config)) {}

Result<Json> HarnessRunner::run_batch(const std::filesystem::path& tasks_file) {
    auto text = read_file(tasks_file);
    if (!text) {
        return fail<Json>(text.error());
    }
    auto parsed = Json::parse(*text);
    if (!parsed) {
        return fail<Json>(parsed.error());
    }
    if (!parsed->is_array()) {
        return fail<Json>("Task file must be a JSON array");
    }
    auto suite_valid = validate_task_suite(*parsed);
    if (!suite_valid) {
        return fail<Json>(suite_valid.error());
    }

    Json report = Json::object();
    Json runs = Json::array();
    int success_count = 0;
    int total = 0;
    for (const auto& task : parsed->as_array()) {
        auto summary = run_one(task);
        ++total;
        Json item = Json::object();
        if (summary) {
            if (summary->success) {
                ++success_count;
            }
            item["id"] = summary->id;
            item["difficulty"] = summary->difficulty;
            item["success"] = summary->success;
            item["score"] = summary->score;
            item["step_count"] = summary->step_count;
            item["tool_call_count"] = summary->tool_call_count;
            item["evaluator"] = summary->evaluator;
            item["message"] = summary->message;
            item["details"] = summary->evaluation_details;
            item["trajectory_path"] = summary->trajectory_path.string();
        } else {
            item["id"] = task.at("id").as_string_or("unknown");
            item["difficulty"] = task.at("difficulty").as_string_or("unknown");
            item["success"] = false;
            item["score"] = 0.0;
            item["message"] = summary.error();
        }
        runs.push_back(std::move(item));
    }

    report["total"] = total;
    report["success_count"] = success_count;
    report["success_rate"] = total == 0 ? 0.0 : static_cast<double>(success_count) / total;
    report["execution_mode"] = config_.prefer_task_script ? "offline_scripted" : "live_llm";
    report["model"] = config_.prefer_task_script
        ? std::string(kScriptedModelLabel)
        : config_.chat_options.model;
    report["runs"] = std::move(runs);
    std::error_code ec;
    std::filesystem::create_directories(config_.output_dir, ec);
    if (ec) {
        return fail<Json>("Cannot create output directory: " + ec.message());
    }
    std::ofstream out(config_.output_dir / "batch_results.json");
    if (!out) {
        return fail<Json>("Cannot write batch results: " + (config_.output_dir / "batch_results.json").string());
    }
    out << report.dump(2) << '\n';
    return report;
}

Result<TaskRunSummary> HarnessRunner::run_one(const Json& task) {
    const std::string id = task.at("id").as_string_or("task");
    const std::string instruction = task.at("instruction").as_string_or(task.at("description").as_string_or());
    if (instruction.empty()) {
        return fail<TaskRunSummary>("Task has no instruction");
    }

    const auto task_workspace = config_.workspace_dir / id;
    if (config_.clean_task_workspace) {
        std::error_code cleanup_ec;
        std::filesystem::remove_all(task_workspace, cleanup_ec);
        if (cleanup_ec) {
            return fail<TaskRunSummary>("Cannot reset task workspace: " + cleanup_ec.message());
        }
    }
    NativeEnvironment environment(task_workspace);
    auto prepared = environment.prepare();
    if (!prepared) {
        return fail<TaskRunSummary>(prepared.error());
    }
    auto seeded = seed_initial_files(task, environment);
    if (!seeded) {
        return fail<TaskRunSummary>(seeded.error());
    }

    ToolExecutionContext tool_context{environment, http_client_};
    auto tools = make_default_tool_registry();

    std::unique_ptr<LLMClient> scripted;
    LLMClient* llm = &llm_;
    const auto scripts = scripted_responses_from_task(task);
    const bool using_task_script = config_.prefer_task_script && !scripts.empty();
    if (using_task_script) {
        scripted = std::make_unique<ScriptedLLMClient>(scripts);
        llm = scripted.get();
    }

    AgentLoop agent(*llm, *tools, skills_, tool_context);
    int hook_count = 0;
    agent.set_step_hook([&hook_count](const StepRecord&) {
        ++hook_count;
    });

    AgentRunConfig run_config;
    run_config.task_id = id;
    run_config.max_steps = static_cast<int>(task.at("max_steps").as_number(8));
    run_config.chat_options = config_.chat_options;
    if (using_task_script) {
        run_config.chat_options.model = std::string(kScriptedModelLabel);
    } else if (!task.at("model").as_string_or().empty()) {
        run_config.chat_options.model = task.at("model").as_string_or();
    }

    auto run = agent.run(instruction, run_config);
    auto evaluator = make_evaluator(task.at("eval_type").as_string_or("keyword"));
    auto evaluation = evaluator->evaluate(task, run.trajectory, environment);
    run.trajectory.success = evaluation.success;
    run.trajectory.final_answer = run.final_answer;

    const auto trajectory_path = config_.output_dir / ("trajectory_" + id + ".json");
    auto saved = run.trajectory.save(trajectory_path);
    if (!saved) {
        return fail<TaskRunSummary>(saved.error());
    }

    TaskRunSummary summary;
    summary.id = id;
    summary.difficulty = task.at("difficulty").as_string_or("unknown");
    summary.success = evaluation.success;
    summary.score = evaluation.score;
    summary.step_count = static_cast<int>(run.trajectory.steps.size());
    for (const auto& step : run.trajectory.steps) {
        if (step.action.type == "tool_call") {
            ++summary.tool_call_count;
        }
    }
    summary.evaluator = evaluator->name();
    summary.message = evaluation.message + " (hooked_steps=" + std::to_string(hook_count) + ")";
    summary.evaluation_details = evaluation.details;
    summary.trajectory_path = trajectory_path;
    return summary;
}

}  // namespace oop
