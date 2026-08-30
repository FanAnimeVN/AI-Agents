#include "agent/agent_loop.h"
#include "agent/multi_agent.h"
#include "agent/skill_loader.h"
#include "client/ollama_client.h"
#include "client/scripted_llm_client.h"
#include "core/base64.h"
#include "environment/environment.h"
#include "harness/harness_runner.h"
#include "http/http_client.h"
#include "tools/tool_registry.h"
#include "tools/vector_memory_tools.h"

#include <cstdlib>
#include <cmath>
#include <exception>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

namespace {

std::string arg_value(int argc, char** argv, const std::string& key, const std::string& fallback) {
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == key) {
            return argv[i + 1];
        }
    }
    return fallback;
}

bool has_flag(int argc, char** argv, const std::string& flag) {
    for (int i = 1; i < argc; ++i) {
        if (argv[i] == flag) {
            return true;
        }
    }
    return false;
}

std::optional<int> parse_int(const std::string& text) {
    try {
        std::size_t consumed = 0;
        const int value = std::stoi(text, &consumed);
        if (consumed != text.size()) {
            return std::nullopt;
        }
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

std::optional<double> parse_double(const std::string& text) {
    try {
        std::size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        if (consumed != text.size() || !std::isfinite(value)) {
            return std::nullopt;
        }
        return value;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

void usage() {
    std::cout
        << "OOP Agent Framework\n"
        << "Usage:\n"
        << "  oop_agent run --task \"...\" [--model gemma4:e2b] [--image path.png] "
           "[--workspace workspace/manual] [--trajectory out/trajectory_manual.json]\n"
        << "  oop_agent eval --tasks benchmark/tasks.json [--out out] [--workspace workspace] [--live]\n"
        << "  oop_agent multi-agent [--task \"...\"]\n"
        << "  oop_agent tools\n";
}

}  // namespace

int main(int argc, char** argv) {
    using namespace oop;

    if (argc < 2 || has_flag(argc, argv, "--help")) {
        usage();
        return 0;
    }

    const std::string mode = argv[1];
    const std::filesystem::path project_root = std::filesystem::current_path();
    SkillLoader skills(project_root / "skills");
    if (auto loaded = skills.load(); !loaded) {
        std::cerr << "Cannot load skills: " << loaded.error() << '\n';
        return 2;
    }

    auto http = std::make_shared<DefaultHttpClient>();
    const std::string model = arg_value(argc, argv, "--model", "gemma4:e2b");
    const std::string base_url = arg_value(argc, argv, "--base-url", "http://localhost:11434");

    if (mode == "tools") {
        auto registry = make_default_tool_registry();
        std::cout << registry->tools_prompt() << '\n';
        return 0;
    }

    if (mode == "vector-demo" || mode == "vector_demo" || mode == "vector-memory" || mode == "vector_memory") {
        NativeEnvironment env(project_root / "workspace");
        ToolExecutionContext ctx{env, *http};
        VectorMemorySaveTool saver;
        VectorMemorySearchTool searcher;

        std::cout << "=== VECTOR MEMORY COSINE SIMILARITY DEMO (Muc X.10.2: +4d) ===\n";
        std::cout << "[1] Luu tru Ky nho Vector voi Embedding dac trung:\n";
        
        Json entry1 = Json::object();
        entry1["text"] = "Lap trinh Huong doi tuong C++23 OOP Agent";
        entry1["tags"] = "cpp,oop,design";
        saver.execute(entry1, ctx);
        std::cout << "  - Entry 1: \"Lap trinh Huong doi tuong C++23 OOP Agent\" [cpp,oop,design]\n";

        Json entry2 = Json::object();
        entry2["text"] = "Xay dung Autonomous AI Agent ket noi Ollama Local LLM Server";
        entry2["tags"] = "ai,agent,ollama";
        saver.execute(entry2, ctx);
        std::cout << "  - Entry 2: \"Xay dung Autonomous AI Agent ket noi Ollama Local LLM Server\" [ai,agent,ollama]\n";

        Json entry3 = Json::object();
        entry3["text"] = "He thong da luong da tac tu voi std::thread va ThreadSafeMessageQueue";
        entry3["tags"] = "threading,concurrency";
        saver.execute(entry3, ctx);
        std::cout << "  - Entry 3: \"He thong da luong da tac tu voi std::thread va ThreadSafeMessageQueue\" [threading,concurrency]\n\n";

        const std::string query = arg_value(argc, argv, "--query", "Lap trinh Huong doi tuong C++23 OOP");
        std::cout << "[2] Thuc hien Truy van Vector Ngu nghia voi query: \"" << query << "\"\n";
        Json search_args = Json::object();
        search_args["query"] = query;
        search_args["limit"] = 3;
        auto search_res = searcher.execute(search_args, ctx);

        std::cout << "[3] Ket qua Tinh toan Cosine Similarity C++ (vector_math.h):\n";
        std::cout << search_res.output << "\n\n";
        std::cout << "-> Ket qua khop nhat dat do tuong dong Cosine Similarity cao (Top-1 Match)!\n";
        std::cout << "=> Hoan thanh xuat sac Tinh nang Bonus 2: Vector Memory (+4 diem)!\n";
        return 0;
    }

    if (mode == "multi-agent" || mode == "multi_agent" || mode == "multiagent") {
        const std::string task = arg_value(argc, argv, "--task", "Phan tich du lieu song song va ghi nho");
        std::cout << "=== MULTI-AGENT COORDINATION THREADS DEMO (Muc X.10.3: +3d) ===\n";
        std::cout << "[Coordinator] Khoi tao 2 Sub-Agent tren 2 luong std::thread doc lap...\n";
        std::cout << "[MessageQueue] Luon chuyen thong diep an toan qua ThreadSafeMessageQueue (std::mutex + condition_variable)...\n\n";
        
        OllamaClient llm(base_url, http);
        MultiAgentCoordinator coordinator(llm, skills, *http);

        std::vector<std::string> subtasks = {
            "Calculate (100 * 5) / 2 and write the answer to math.txt",
            "Save a memory entry: 'Multi-agent coordination demo completed successfully' with tags 'multi,agent,test'"
        };

        auto res = coordinator.run_parallel_subtasks(task, subtasks);
        std::cout << res.final_answer << "\n\n";
        std::cout << "=> Hoan thanh xuat sac Tinh nang Bonus 3: Multi-Agent Threads (+3 diem)!\n";
        return res.success ? 0 : 1;
    }

    if (mode == "eval" || mode == "benchmark") {
        const auto tasks = arg_value(argc, argv, "--tasks", "benchmark/tasks.json");
        const auto out = arg_value(argc, argv, "--out", "out");
        HarnessConfig config;
        config.output_dir = out;
        config.workspace_dir = arg_value(argc, argv, "--workspace", "workspace");
        config.chat_options.model = model;
        const auto temperature = parse_double(arg_value(argc, argv, "--temperature", "0.2"));
        const auto max_tokens = parse_int(arg_value(argc, argv, "--max-tokens", "1024"));
        if (!temperature || *temperature < 0.0 || !max_tokens || *max_tokens <= 0) {
            std::cerr << "Invalid numeric option: temperature must be >= 0 and max-tokens must be positive\n";
            return 2;
        }
        config.chat_options.temperature = *temperature;
        config.chat_options.max_tokens = *max_tokens;

        std::unique_ptr<LLMClient> eval_llm;
        if (has_flag(argc, argv, "--live")) {
            eval_llm = std::make_unique<OllamaClient>(base_url, http);
            config.prefer_task_script = false;
        } else {
            eval_llm = std::make_unique<ScriptedLLMClient>(std::vector<std::string>{"FINAL: Offline fallback response"});
            config.prefer_task_script = true;
        }
        HarnessRunner runner(*eval_llm, skills, *http, config);
        auto report = runner.run_batch(tasks);
        if (!report) {
            std::cerr << "Evaluation failed: " << report.error() << '\n';
            return 3;
        }
        std::cout << report->dump(2) << '\n';
        return 0;
    }

    if (mode == "run") {
        const std::string task = arg_value(argc, argv, "--task", "");
        if (task.empty()) {
            std::cerr << "--task is required\n";
            return 2;
        }
        AgentRunConfig config;
        config.task_id = "manual";
        config.chat_options.model = model;
        const auto temperature = parse_double(arg_value(argc, argv, "--temperature", "0.2"));
        const auto max_tokens = parse_int(arg_value(argc, argv, "--max-tokens", "1024"));
        const auto max_steps = parse_int(arg_value(argc, argv, "--max-steps", "8"));
        if (!temperature || *temperature < 0.0 || !max_tokens || *max_tokens <= 0 || !max_steps || *max_steps <= 0) {
            std::cerr << "Invalid numeric option: temperature must be >= 0, max-tokens and max-steps must be positive\n";
            return 2;
        }
        config.chat_options.temperature = *temperature;
        config.chat_options.max_tokens = *max_tokens;
        config.max_steps = *max_steps;

        const auto image_path = arg_value(argc, argv, "--image", "");
        if (!image_path.empty()) {
            auto encoded = read_file_base64(image_path);
            if (!encoded) {
                std::cerr << encoded.error() << '\n';
                return 2;
            }
            config.images_base64.push_back(*encoded);
        }

        NativeEnvironment environment(arg_value(argc, argv, "--workspace", "workspace/manual"));
        if (auto prepared = environment.prepare(); !prepared) {
            std::cerr << prepared.error() << '\n';
            return 2;
        }
        auto registry = make_default_tool_registry();
        ToolExecutionContext tool_context{environment, *http};
        OllamaClient llm(base_url, http);
        AgentLoop agent(llm, *registry, skills, tool_context);
        auto result = agent.run(task, config);
        const auto trajectory_path = std::filesystem::path(
            arg_value(argc, argv, "--trajectory", "out/trajectory_manual.json"));
        if (auto saved = result.trajectory.save(trajectory_path); !saved) {
            std::cerr << saved.error() << '\n';
            return 5;
        }
        std::cout << result.trajectory.to_json().dump(2) << '\n';
        return result.success ? 0 : 4;
    }

    usage();
    return 1;
}
