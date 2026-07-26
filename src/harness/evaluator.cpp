#include "harness/evaluator.h"

#include "core/string_utils.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <sstream>

namespace oop {
namespace {

std::string read_text_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<std::string> tool_calls(const Trajectory& trajectory) {
    std::vector<std::string> calls;
    for (const auto& step : trajectory.steps) {
        if (step.action.type == "tool_call") {
            calls.push_back(step.action.tool);
        }
    }
    return calls;
}

bool matches_tool_event(const StepRecord& step, const Json& event) {
    if (step.action.type != "tool_call") {
        return false;
    }

    const auto tool = event.at("tool").as_string_or();
    if (!tool.empty() && step.action.tool != tool) {
        return false;
    }

    const auto args_contains = event.at("args_contains").as_string_or();
    if (!args_contains.empty() && !str::contains_ci(step.action.args.dump(), args_contains)) {
        return false;
    }

    const auto result_contains = event.at("result_contains").as_string_or();
    return result_contains.empty() || str::contains_ci(step.tool_result, result_contains);
}

bool matches_ordered_event_pair(
    const Trajectory& trajectory,
    const Json& pair,
    std::vector<std::string>& failures) {
    const auto first = pair.at("first");
    const auto second = pair.at("then");
    if (!first.is_object() || !second.is_object()) {
        failures.push_back("ordered_event_pairs contains a non-object pair");
        return false;
    }

    std::size_t first_index = 0;
    bool found_first = false;
    for (std::size_t i = 0; i < trajectory.steps.size(); ++i) {
        if (matches_tool_event(trajectory.steps[i], first)) {
            first_index = i;
            found_first = true;
            break;
        }
    }
    if (!found_first) {
        failures.push_back("Ordered prerequisite event was not observed");
        return false;
    }

    for (std::size_t i = first_index + 1; i < trajectory.steps.size(); ++i) {
        if (matches_tool_event(trajectory.steps[i], second)) {
            return true;
        }
    }
    failures.push_back("Ordered dependent event occurred before its prerequisite or was missing");
    return false;
}

void add_contract_failures(
    const Json& task,
    const Trajectory& trajectory,
    const Environment& environment,
    std::vector<std::string>& failures) {
    const auto calls = tool_calls(trajectory);

    if (task.contains("min_steps")) {
        const auto minimum = static_cast<std::size_t>(std::max(0.0, task.at("min_steps").as_number()));
        if (trajectory.steps.size() < minimum) {
            failures.push_back("Trajectory has fewer than min_steps=" + std::to_string(minimum));
        }
    }

    if (task.contains("min_tool_calls")) {
        const auto minimum = static_cast<std::size_t>(std::max(0.0, task.at("min_tool_calls").as_number()));
        if (calls.size() < minimum) {
            failures.push_back("Trajectory has fewer than min_tool_calls=" + std::to_string(minimum));
        }
    }

    if (task.contains("max_tool_calls")) {
        const auto maximum = static_cast<std::size_t>(std::max(0.0, task.at("max_tool_calls").as_number()));
        if (calls.size() > maximum) {
            failures.push_back("Trajectory has more than max_tool_calls=" + std::to_string(maximum));
        }
    }

    if (task.contains("required_tools") && task.at("required_tools").is_array()) {
        for (const auto& item : task.at("required_tools").as_array()) {
            const auto required = item.as_string_or();
            if (!required.empty() && std::find(calls.begin(), calls.end(), required) == calls.end()) {
                failures.push_back("Required tool was not called: " + required);
            }
        }
    }

    if (task.contains("required_tool_order") && task.at("required_tool_order").is_array()) {
        std::size_t cursor = 0;
        for (const auto& item : task.at("required_tool_order").as_array()) {
            const auto required = item.as_string_or();
            const auto found = std::find(calls.begin() + static_cast<std::ptrdiff_t>(cursor), calls.end(), required);
            if (found == calls.end()) {
                failures.push_back("Required tool order was not observed at: " + required);
                break;
            }
            cursor = static_cast<std::size_t>(std::distance(calls.begin(), found)) + 1;
        }
    }

    if (task.contains("required_action_types") && task.at("required_action_types").is_array()) {
        for (const auto& item : task.at("required_action_types").as_array()) {
            const auto required = item.as_string_or();
            const auto found = std::find_if(
                trajectory.steps.begin(), trajectory.steps.end(),
                [&required](const StepRecord& step) { return step.action.type == required; });
            if (!required.empty() && found == trajectory.steps.end()) {
                failures.push_back("Required action type was not observed: " + required);
            }
        }
    }

    if (task.contains("expected_observations") && task.at("expected_observations").is_array()) {
        for (const auto& item : task.at("expected_observations").as_array()) {
            const auto expected = item.as_string_or();
            const auto found = std::find_if(
                trajectory.steps.begin(), trajectory.steps.end(),
                [&expected](const StepRecord& step) { return str::contains_ci(step.tool_result, expected); });
            if (!expected.empty() && found == trajectory.steps.end()) {
                failures.push_back("Expected observation was not recorded: " + expected);
            }
        }
    }

    if (task.contains("expected_tool_events") && task.at("expected_tool_events").is_array()) {
        const auto order_mode = task.at("expected_tool_event_order").as_string_or("sequential");
        if (order_mode == "any") {
            std::vector<bool> used(trajectory.steps.size(), false);
            for (const auto& event : task.at("expected_tool_events").as_array()) {
                bool matched = false;
                for (std::size_t index = 0; index < trajectory.steps.size(); ++index) {
                    if (!used[index] && matches_tool_event(trajectory.steps[index], event)) {
                        used[index] = true;
                        matched = true;
                        break;
                    }
                }
                if (!matched) {
                    const auto tool = event.at("tool").as_string_or("<any tool>");
                    failures.push_back("Expected tool event was not observed: " + tool);
                }
            }
        } else {
            std::size_t cursor = 0;
            for (const auto& event : task.at("expected_tool_events").as_array()) {
                const auto found = std::find_if(
                    trajectory.steps.begin() + static_cast<std::ptrdiff_t>(cursor),
                    trajectory.steps.end(),
                    [&event](const StepRecord& step) { return matches_tool_event(step, event); });
                if (found == trajectory.steps.end()) {
                    const auto tool = event.at("tool").as_string_or("<any tool>");
                    failures.push_back("Expected tool event was not observed: " + tool);
                    break;
                }
                cursor = static_cast<std::size_t>(std::distance(trajectory.steps.begin(), found)) + 1;
            }
        }
    }

    if (task.contains("ordered_event_pairs") && task.at("ordered_event_pairs").is_array()) {
        for (const auto& pair : task.at("ordered_event_pairs").as_array()) {
            matches_ordered_event_pair(trajectory, pair, failures);
        }
    }

    if (task.contains("forbidden_paths") && task.at("forbidden_paths").is_array()) {
        for (const auto& item : task.at("forbidden_paths").as_array()) {
            const auto path = item.as_string_or();
            if (!path.empty() && environment.resolve_inside_workspace(path)) {
                failures.push_back("Forbidden path was accepted: " + path);
            }
        }
    }
}

}  // namespace

std::string KeywordEvaluator::name() const {
    return "keyword";
}

EvaluationResult KeywordEvaluator::evaluate(
    const Json& task,
    const Trajectory& trajectory,
    const Environment& environment) {
    std::vector<std::string> failures;
    if (!trajectory.success) {
        failures.push_back("Agent did not finish successfully");
    }
    add_contract_failures(task, trajectory, environment, failures);
    if (task.contains("expected_keywords") && task.at("expected_keywords").is_array()) {
        for (const auto& keyword : task.at("expected_keywords").as_array()) {
            const auto text = keyword.as_string_or();
            if (!text.empty() && !str::contains_ci(trajectory.final_answer, text)) {
                failures.push_back("Final answer missing keyword: " + text);
            }
        }
    }

    const bool ok = failures.empty();
    Json details = Json::object();
    Json failures_json = Json::array();
    for (const auto& item : failures) {
        failures_json.push_back(item);
    }
    details["failures"] = std::move(failures_json);
    return EvaluationResult{
        ok,
        ok ? 1.0 : 0.0,
        ok ? "All keyword and trajectory checks passed" : "Keyword or trajectory checks failed",
        details};
}

std::string FunctionalEvaluator::name() const {
    return "functional";
}

EvaluationResult FunctionalEvaluator::evaluate(
    const Json& task,
    const Trajectory& trajectory,
    const Environment& environment) {
    Json details = Json::object();
    std::vector<std::string> failure_list;

    if (!trajectory.success) {
        failure_list.push_back("Agent did not finish successfully");
    }
    add_contract_failures(task, trajectory, environment, failure_list);

    if (task.contains("expected_files") && task.at("expected_files").is_array()) {
        for (const auto& file_check : task.at("expected_files").as_array()) {
            const std::string path = file_check.at("path").as_string_or();
            const std::string contains = file_check.at("contains").as_string_or();
            auto resolved = environment.resolve_inside_workspace(path);
            if (!resolved) {
                failure_list.push_back(resolved.error());
                continue;
            }
            if (!std::filesystem::exists(*resolved)) {
                failure_list.push_back("Missing file: " + path);
                continue;
            }
            const auto content = read_text_file(*resolved);
            if (!contains.empty() && !str::contains_ci(content, contains)) {
                failure_list.push_back("File " + path + " does not contain: " + contains);
            }
            if (file_check.contains("contains_all") && file_check.at("contains_all").is_array()) {
                for (const auto& required : file_check.at("contains_all").as_array()) {
                    const auto text = required.as_string_or();
                    if (!text.empty() && !str::contains_ci(content, text)) {
                        failure_list.push_back("File " + path + " does not contain all required text: " + text);
                    }
                }
            }
            if (file_check.contains("exact")) {
                const auto expected = file_check.at("exact").as_string_or();
                if (content != expected) {
                    failure_list.push_back("File " + path + " does not match the exact expected content");
                }
            }
            const std::string not_contains = file_check.at("not_contains").as_string_or();
            if (!not_contains.empty() && str::contains_ci(content, not_contains)) {
                failure_list.push_back("File " + path + " unexpectedly contains: " + not_contains);
            }
        }
    }

    if (task.contains("expected_keywords") && task.at("expected_keywords").is_array()) {
        for (const auto& keyword : task.at("expected_keywords").as_array()) {
            const auto text = keyword.as_string_or();
            if (!text.empty() && !str::contains_ci(trajectory.final_answer, text)) {
                failure_list.push_back("Final answer missing keyword: " + text);
            }
        }
    }

    Json failures = Json::array();
    for (const auto& failure : failure_list) {
        failures.push_back(failure);
    }
    const bool ok = failure_list.empty();
    details["failures"] = std::move(failures);
    return EvaluationResult{ok, ok ? 1.0 : 0.0, ok ? "Functional checks passed" : "Functional checks failed", details};
}

std::string VLMEvaluator::name() const {
    return "vlm";
}

EvaluationResult VLMEvaluator::evaluate(
    const Json& task,
    const Trajectory& trajectory,
    const Environment& environment) {
    (void)environment;
    const bool saw_image = trajectory.to_json().dump().find("images_base64") != std::string::npos ||
                           str::contains_ci(task.dump(), "image");
    return EvaluationResult{
        saw_image,
        saw_image ? 1.0 : 0.5,
        saw_image ? "VLM-related evidence detected" : "VLM evaluator stub: no image evidence in offline run",
        Json::object()};
}

std::unique_ptr<Evaluator> make_evaluator(const std::string& type) {
    if (type == "functional") {
        return std::make_unique<FunctionalEvaluator>();
    }
    if (type == "vlm") {
        return std::make_unique<VLMEvaluator>();
    }
    return std::make_unique<KeywordEvaluator>();
}

}  // namespace oop
