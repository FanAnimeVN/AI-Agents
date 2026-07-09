#pragma once

#include "client/llm_client.h"

#include <deque>

namespace oop {

class ScriptedLLMClient final : public LLMClient {
public:
    explicit ScriptedLLMClient(std::vector<std::string> responses);

    Result<ChatResponse> chat(const ChatRequest& request) override;
    [[nodiscard]] std::string name() const override;

private:
    std::deque<std::string> responses_;
};

}  // namespace oop
