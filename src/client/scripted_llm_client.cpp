#include "client/scripted_llm_client.h"

namespace oop {

ScriptedLLMClient::ScriptedLLMClient(std::vector<std::string> responses)
    : responses_(responses.begin(), responses.end()) {}

Result<ChatResponse> ScriptedLLMClient::chat(const ChatRequest& request) {
    (void)request;
    if (responses_.empty()) {
        return ChatResponse{"FINAL: No scripted response left.", 12, 0, Json::object()};
    }
    auto content = responses_.front();
    responses_.pop_front();
    return ChatResponse{std::move(content), 24, 0, Json::object()};
}

std::string ScriptedLLMClient::name() const {
    return "scripted";
}

}  // namespace oop
