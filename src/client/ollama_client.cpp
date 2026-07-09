#include "client/ollama_client.h"

#include <chrono>

namespace oop {

namespace {

std::string without_trailing_slash(std::string url) {
    while (url.size() > 1 && url.back() == '/') {
        url.pop_back();
    }
    return url;
}

}  // namespace

OllamaClient::OllamaClient(std::string base_url, std::shared_ptr<HttpClient> http_client)
    : base_url_(without_trailing_slash(std::move(base_url))), http_(std::move(http_client)) {
    if (!http_) {
        http_ = std::make_shared<DefaultHttpClient>();
    }
}

Result<ChatResponse> OllamaClient::chat(const ChatRequest& request) {
    Json body = Json::object();
    body["model"] = request.options.model;
    body["stream"] = false;
    body["options"] = Json::object();
    body["options"]["temperature"] = request.options.temperature;
    body["options"]["num_predict"] = request.options.max_tokens;

    Json messages = Json::array();
    for (const auto& msg : request.messages) {
        Json item = Json::object();
        item["role"] = msg.role;
        item["content"] = msg.content;
        if (!msg.images_base64.empty()) {
            Json images = Json::array();
            for (const auto& image : msg.images_base64) {
                images.push_back(image);
            }
            item["images"] = std::move(images);
        }
        messages.push_back(std::move(item));
    }
    body["messages"] = std::move(messages);

    const auto start = std::chrono::steady_clock::now();
    auto response = http_->post_json(base_url_ + "/api/chat", body.dump(), {}, 90);
    const auto end = std::chrono::steady_clock::now();
    if (!response) {
        return fail<ChatResponse>(response.error());
    }
    if (response->status_code < 200 || response->status_code >= 300) {
        return fail<ChatResponse>("Ollama returned HTTP " + std::to_string(response->status_code) + ": " + response->body);
    }

    auto parsed = Json::parse(response->body);
    if (!parsed) {
        return fail<ChatResponse>("Malformed Ollama JSON response: " + parsed.error());
    }

    ChatResponse out;
    out.raw = *parsed;
    out.content = parsed->at("message").at("content").as_string_or();
    if (out.content.empty()) {
        out.content = parsed->at("message").at("thinking").as_string_or();
    }
    out.tokens_used = static_cast<int>(
        parsed->at("eval_count").as_number(0) + parsed->at("prompt_eval_count").as_number(0));
    out.latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    if (out.content.empty()) {
        return fail<ChatResponse>("Ollama response did not contain message.content");
    }
    return out;
}

std::string OllamaClient::name() const {
    return "ollama(" + base_url_ + ")";
}

}  // namespace oop
