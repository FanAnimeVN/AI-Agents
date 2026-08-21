#include "agent/multi_agent.h"

#include "environment/environment.h"
#include "tools/tool_registry.h"

#include <iostream>
#include <sstream>
#include <thread>

namespace oop {

MultiAgentCoordinator::MultiAgentCoordinator(LLMClient& llm, SkillLoader& skills, HttpClient& http_client)
    : llm_(llm), skills_(skills), http_client_(http_client) {}

SubAgentResult MultiAgentCoordinator::run_parallel_subtasks(
    const std::string& parent_task,
    const std::vector<std::string>& subtask_prompts) {

    SubAgentResult aggregated;
    aggregated.agent_id = "multi_agent_coordinator";
    aggregated.success = true;

    if (subtask_prompts.empty()) {
        aggregated.final_answer = "No subtasks provided for parent task: " + parent_task;
        return aggregated;
    }

    std::vector<std::thread> worker_threads;
    std::vector<SubAgentResult> results(subtask_prompts.size());

    for (std::size_t i = 0; i < subtask_prompts.size(); ++i) {
        const std::string agent_id = "sub_agent_" + std::to_string(i + 1);
        const std::string prompt = subtask_prompts[i];

        worker_threads.emplace_back([this, agent_id, prompt, i, &results] {
            NativeEnvironment env("workspace/" + agent_id);
            (void)env.prepare();

            auto registry = make_default_tool_registry();
            ToolExecutionContext tool_context{env, http_client_};
            AgentLoop agent(llm_, *registry, skills_, tool_context);

            AgentRunConfig config;
            config.task_id = agent_id;
            config.max_steps = 5;

            // Notify message queue about sub-agent start
            message_queue_.push(AgentMessage{agent_id, "coordinator", "Started subtask: " + prompt});

            auto run_res = agent.run(prompt, config);

            SubAgentResult res;
            res.agent_id = agent_id;
            res.success = run_res.success;
            res.final_answer = run_res.final_answer.empty() ? "Completed" : run_res.final_answer;
            res.steps_taken = static_cast<int>(run_res.trajectory.steps.size());

            results[i] = res;

            message_queue_.push(AgentMessage{agent_id, "coordinator", "Finished subtask with answer: " + res.final_answer});
        });
    }

    // Join all worker threads
    for (auto& thread : worker_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    std::ostringstream summary;
    summary << "Multi-Agent Coordination Summary for Task: \"" << parent_task << "\"\n";
    for (std::size_t i = 0; i < results.size(); ++i) {
        summary << "- Sub-Agent " << i + 1 << " [" << results[i].agent_id << "]: "
                << (results[i].success ? "SUCCESS" : "FAILED")
                << " | Steps: " << results[i].steps_taken
                << " | Output: " << results[i].final_answer << "\n";
        if (!results[i].success) {
            aggregated.success = false;
        }
    }

    aggregated.final_answer = summary.str();
    return aggregated;
}

}  // namespace oop
