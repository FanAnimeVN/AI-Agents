#include "tools/web_search_tool.h"

#include "core/string_utils.h"

#include <sstream>

namespace oop {
namespace {

std::string url_encode(std::string_view value) {
    std::ostringstream out;
    constexpr char hex[] = "0123456789ABCDEF";
    for (const unsigned char c : value) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out << static_cast<char>(c);
        } else {
            out << '%' << hex[c >> 4] << hex[c & 15];
        }
    }
    return out.str();
}

}  // namespace

std::string WebSearchTool::name() const {
    return "web_search";
}

std::string WebSearchTool::description() const {
    return "Search the web through DuckDuckGo Instant Answer API. Args: {\"query\":\"...\"}.";
}

ToolResult WebSearchTool::execute(const Json& args, ToolExecutionContext& context) {
    const std::string query = args.is_string() ? args.as_string() : args.at("query").as_string_or();
    if (query.empty()) {
        return ToolResult{false, {}, "Missing query", Json::object()};
    }
    const std::string url = "https://api.duckduckgo.com/?q=" + url_encode(query) + "&format=json&no_redirect=1&no_html=1";
    auto response = context.http_client.get(url, {{"User-Agent", "OOP-Agent-Framework/1.0"}}, 20);
    if (!response) {
        return ToolResult{false, {}, response.error(), Json::object()};
    }
    if (response->status_code < 200 || response->status_code >= 300) {
        return ToolResult{false, {}, "Search HTTP " + std::to_string(response->status_code), Json::object()};
    }
    auto parsed = Json::parse(response->body);
    if (!parsed) {
        return ToolResult{false, {}, "Malformed search JSON: " + parsed.error(), Json::object()};
    }
    std::string answer = parsed->at("AbstractText").as_string_or();
    if (answer.empty()) {
        answer = parsed->at("Heading").as_string_or("No direct abstract returned.");
    }
    Json meta = Json::object();
    meta["query"] = query;
    meta["source"] = "DuckDuckGo Instant Answer API";
    return ToolResult{true, answer, {}, meta};
}

}  // namespace oop
