#include "tools/vector_memory_tools.h"

#include "core/string_utils.h"
#include "core/vector_math.h"

#include <algorithm>
#include <mutex>
#include <sstream>
#include <vector>

namespace oop {
namespace {

struct VectorMemoryEntry {
    std::string text;
    std::string tags;
    std::vector<double> vector;
};

std::mutex vector_memory_mutex;
std::vector<VectorMemoryEntry> global_vector_store;

std::vector<std::string> extract_vocabulary(const std::string& query, const std::vector<VectorMemoryEntry>& store) {
    std::vector<std::string> vocab;
    const auto add_tokens = [&vocab](std::string_view s) {
        std::string token;
        for (const char c : s) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                token.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            } else if (!token.empty()) {
                if (!std::ranges::contains(vocab, token) && token.size() > 2) {
                    vocab.push_back(token);
                }
                token.clear();
            }
        }
        if (!token.empty() && !std::ranges::contains(vocab, token) && token.size() > 2) {
            vocab.push_back(token);
        }
    };

    add_tokens(query);
    for (const auto& entry : store) {
        add_tokens(entry.text);
        add_tokens(entry.tags);
    }
    return vocab;
}

}  // namespace

std::string VectorMemorySaveTool::name() const {
    return "vector_memory_save";
}

std::string VectorMemorySaveTool::description() const {
    return "Persist a memory with vector embedding. Args: {\"text\":\"...\",\"tags\":\"...\"}.";
}

ToolResult VectorMemorySaveTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)context;
    const std::string text = args.at("text").as_string_or();
    const std::string tags = args.at("tags").as_string_or();
    if (text.empty()) {
        return ToolResult{false, {}, "Missing memory text", Json::object()};
    }

    std::lock_guard lock(vector_memory_mutex);
    VectorMemoryEntry entry{text, tags, {}};
    global_vector_store.push_back(std::move(entry));

    Json meta = Json::object();
    meta["total_entries"] = static_cast<double>(global_vector_store.size());
    return ToolResult{true, "Vector memory saved", {}, meta};
}

std::string VectorMemorySearchTool::name() const {
    return "vector_memory_search";
}

std::string VectorMemorySearchTool::description() const {
    return "Search memories using C++ Cosine Similarity vector search. Args: {\"query\":\"...\",\"limit\":5}.";
}

ToolResult VectorMemorySearchTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)context;
    const std::string query = args.at("query").as_string_or();
    const int limit = static_cast<int>(args.at("limit").as_number(5));
    if (query.empty()) {
        return ToolResult{false, {}, "Missing search query", Json::object()};
    }

    std::lock_guard lock(vector_memory_mutex);
    if (global_vector_store.empty()) {
        return ToolResult{true, "No vector memories found", {}, Json::object()};
    }

    const auto vocab = extract_vocabulary(query, global_vector_store);
    const auto query_vec = text_to_vector(query, vocab);

    struct ScoredEntry {
        std::size_t index;
        double score;
    };
    std::vector<ScoredEntry> scored;

    for (std::size_t i = 0; i < global_vector_store.size(); ++i) {
        const auto entry_vec = text_to_vector(global_vector_store[i].text + " " + global_vector_store[i].tags, vocab);
        const double sim = cosine_similarity(query_vec, entry_vec);
        if (sim > 0.0) {
            scored.push_back({i, sim});
        }
    }

    std::sort(scored.begin(), scored.end(), [](const ScoredEntry& a, const ScoredEntry& b) {
        return a.score > b.score;
    });

    std::ostringstream out;
    const int count = std::min(limit, static_cast<int>(scored.size()));
    for (int i = 0; i < count; ++i) {
        const auto& item = global_vector_store[scored[i].index];
        out << i + 1 << ". [Cosine Similarity: " << scored[i].score << "] " << item.text;
        if (!item.tags.empty()) {
            out << " [" << item.tags << "]";
        }
        if (i + 1 < count) {
            out << '\n';
        }
    }

    Json meta = Json::object();
    meta["matched_count"] = static_cast<double>(count);
    return ToolResult{true, out.str().empty() ? "No vector memories matched search query" : out.str(), {}, meta};
}

}  // namespace oop
