#include "tools/memory_tools.h"

#include "core/string_utils.h"

#ifdef OOP_HAVE_SQLITE
#include <sqlite3.h>
#endif

#include <mutex>
#include <map>
#include <sstream>
#include <vector>

namespace oop {
namespace {

struct MemoryEntry {
    std::string text;
    std::string tags;
};

#ifndef OOP_HAVE_SQLITE
std::mutex fallback_mutex;
std::map<std::string, std::vector<MemoryEntry>> fallback_memory_by_workspace;

std::string workspace_key(const Environment& environment) {
    return environment.workspace_root().lexically_normal().string();
}
#endif

#ifdef OOP_HAVE_SQLITE
class SqliteDb {
public:
    explicit SqliteDb(const std::filesystem::path& path) {
        if (sqlite3_open(path.string().c_str(), &db_) != SQLITE_OK) {
            error_ = sqlite3_errmsg(db_);
            return;
        }
        exec("CREATE TABLE IF NOT EXISTS memories ("
             "id INTEGER PRIMARY KEY AUTOINCREMENT,"
             "text TEXT NOT NULL,"
             "tags TEXT,"
             "created_at TEXT DEFAULT CURRENT_TIMESTAMP)");
    }

    ~SqliteDb() {
        if (db_ != nullptr) {
            sqlite3_close(db_);
        }
    }

    [[nodiscard]] bool ok() const {
        return error_.empty();
    }

    [[nodiscard]] std::string error() const {
        return error_;
    }

    void exec(const std::string& sql) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
            error_ = err == nullptr ? "sqlite exec failed" : err;
            sqlite3_free(err);
        }
    }

    Result<void> save(const std::string& text, const std::string& tags) {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT INTO memories(text, tags) VALUES (?, ?)";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return std::unexpected(sqlite3_errmsg(db_));
        }
        sqlite3_bind_text(stmt, 1, text.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, tags.c_str(), -1, SQLITE_TRANSIENT);
        const int code = sqlite3_step(stmt);
        sqlite3_finalize(stmt);
        if (code != SQLITE_DONE) {
            return std::unexpected(sqlite3_errmsg(db_));
        }
        return {};
    }

    Result<std::vector<MemoryEntry>> search(const std::string& query, int limit) {
        std::vector<MemoryEntry> rows;
        sqlite3_stmt* stmt = nullptr;
        const char* sql =
            "SELECT text, tags FROM memories "
            "WHERE lower(text) LIKE lower(?) OR lower(tags) LIKE lower(?) "
            "ORDER BY id DESC LIMIT ?";
        if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            return fail<std::vector<MemoryEntry>>(sqlite3_errmsg(db_));
        }
        const std::string pattern = "%" + query + "%";
        sqlite3_bind_text(stmt, 1, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, pattern.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, limit);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            rows.push_back(MemoryEntry{
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)),
                reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1))});
        }
        sqlite3_finalize(stmt);
        return rows;
    }

private:
    sqlite3* db_ = nullptr;
    std::string error_;
};
#endif

std::string entries_to_text(const std::vector<MemoryEntry>& entries) {
    std::ostringstream out;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        out << i + 1 << ". " << entries[i].text;
        if (!entries[i].tags.empty()) {
            out << " [" << entries[i].tags << "]";
        }
        if (i + 1 < entries.size()) {
            out << '\n';
        }
    }
    return out.str();
}

}  // namespace

std::string MemorySaveTool::name() const {
    return "memory_save";
}

std::string MemorySaveTool::description() const {
    return "Persist a memory in SQLite. Args: {\"text\":\"...\",\"tags\":\"comma,separated\"}.";
}

ToolResult MemorySaveTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)context;
    const std::string text = args.at("text").as_string_or();
    const std::string tags = args.at("tags").as_string_or();
    if (text.empty()) {
        return ToolResult{false, {}, "Missing memory text", Json::object()};
    }
#ifdef OOP_HAVE_SQLITE
    SqliteDb db(context.environment.workspace_root() / "memory.sqlite");
    if (!db.ok()) {
        return ToolResult{false, {}, db.error(), Json::object()};
    }
    auto saved = db.save(text, tags);
    if (!saved) {
        return ToolResult{false, {}, saved.error(), Json::object()};
    }
#else
    std::lock_guard lock(fallback_mutex);
    fallback_memory_by_workspace[workspace_key(context.environment)].push_back(MemoryEntry{text, tags});
#endif
    return ToolResult{true, "Memory saved", {}, Json::object()};
}

std::string MemorySearchTool::name() const {
    return "memory_search";
}

std::string MemorySearchTool::description() const {
    return "Search saved memories by keyword. Args: {\"query\":\"...\",\"limit\":5}.";
}

ToolResult MemorySearchTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)context;
    const std::string query = args.at("query").as_string_or();
    const int limit = static_cast<int>(args.at("limit").as_number(5));
    if (query.empty()) {
        return ToolResult{false, {}, "Missing query", Json::object()};
    }
    if (limit <= 0) {
        return ToolResult{false, {}, "Limit must be positive", Json::object()};
    }
    std::vector<MemoryEntry> matches;
#ifdef OOP_HAVE_SQLITE
    SqliteDb db(context.environment.workspace_root() / "memory.sqlite");
    if (!db.ok()) {
        return ToolResult{false, {}, db.error(), Json::object()};
    }
    auto result = db.search(query, limit);
    if (!result) {
        return ToolResult{false, {}, result.error(), Json::object()};
    }
    matches = *result;
#else
    std::lock_guard lock(fallback_mutex);
    const auto memory_it = fallback_memory_by_workspace.find(workspace_key(context.environment));
    if (memory_it == fallback_memory_by_workspace.end()) {
        return ToolResult{true, "No memories found", {}, Json::object()};
    }
    for (const auto& item : memory_it->second) {
        if (str::contains_ci(item.text, query) || str::contains_ci(item.tags, query)) {
            matches.push_back(item);
        }
        if (static_cast<int>(matches.size()) >= limit) {
            break;
        }
    }
#endif
    Json meta = Json::object();
    meta["count"] = static_cast<double>(matches.size());
    return ToolResult{true, matches.empty() ? "No memories found" : entries_to_text(matches), {}, meta};
}

}  // namespace oop
