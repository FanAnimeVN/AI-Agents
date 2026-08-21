#pragma once

#include "agent/agent_loop.h"
#include "agent/message_queue.h"

#include <string>
#include <vector>

namespace oop {

struct AgentMessage {
    std::string sender_id;
    std::string receiver_id;
    std::string content;
};

struct SubAgentResult {
    std::string agent_id;
    bool success = false;
    std::string final_answer;
    int steps_taken = 0;
};

class MultiAgentCoordinator {
public:
    MultiAgentCoordinator(LLMClient& llm, SkillLoader& skills, HttpClient& http_client);

    SubAgentResult run_parallel_subtasks(
        const std::string& parent_task,
        const std::vector<std::string>& subtask_prompts);

private:
    LLMClient& llm_;
    SkillLoader& skills_;
    HttpClient& http_client_;
    ThreadSafeMessageQueue<AgentMessage> message_queue_;
};

}  // namespace oop
