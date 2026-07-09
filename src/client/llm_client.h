#pragma once

#include "core/json.h"
#include "core/result.h"

#include <optional>
#include <string>
#include <vector>

namespace oop {

struct ChatMessage {
    std::string role;
    std::string content;
    std::vector<std::string> images_base64;
};

struct ChatOptions {
    std::string model = "gemma4:e2b";
    double temperature = 0.2;
    int max_tokens = 1024;
};

struct ChatRequest {
    std::vector<ChatMessage> messages;
    ChatOptions options;
};

struct ChatResponse {
    std::string content;
    int tokens_used = 0;
    long long latency_ms = 0;
    Json raw = Json::object();
};

class LLMClient {
public:
    virtual ~LLMClient() = default;
    virtual Result<ChatResponse> chat(const ChatRequest& request) = 0;
    [[nodiscard]] virtual std::string name() const = 0;
};

}  // namespace oop
