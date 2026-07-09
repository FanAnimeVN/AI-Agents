#pragma once

#include "client/llm_client.h"
#include "http/http_client.h"

#include <memory>

namespace oop {

class OllamaClient final : public LLMClient {
public:
    OllamaClient(std::string base_url, std::shared_ptr<HttpClient> http_client);

    Result<ChatResponse> chat(const ChatRequest& request) override;
    [[nodiscard]] std::string name() const override;

private:
    std::string base_url_;
    std::shared_ptr<HttpClient> http_;
};

}  // namespace oop
