#include "agent/agent_loop.h"
#include "agent/loop_detector.h"
#include "core/cpp26_preview.h"
#include "agent/skill_loader.h"
#include "core/base64.h"
#include "core/json.h"
#include "core/json_library_probe.h"
#include "client/ollama_client.h"
#include "client/scripted_llm_client.h"
#include "environment/environment.h"
#include "harness/evaluator.h"
#include "http/http_client.h"
#include "tools/calculator_tool.h"
#include "tools/file_tools.h"
#include "tools/memory_tools.h"
#include "tools/tool_registry.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <map>
#include <thread>
#include <utility>

using namespace oop;

static_assert(std::has_virtual_destructor_v<AgentLoop>);

void check(bool condition, std::string_view expression, int line) {
    if (!condition) {
        throw std::runtime_error(
            "Check failed at line " + std::to_string(line) + ": " + std::string(expression));
    }
}

#define CHECK(expression) check(static_cast<bool>(expression), #expression, __LINE__)

class CapturingHttpClient final : public HttpClient {
public:
    Result<HttpResponse> post_json(
        const std::string& url,
        const std::string& body,
        const std::map<std::string, std::string>& headers,
        int timeout_seconds) override {
        (void)headers;
        (void)timeout_seconds;
        last_url = url;
        last_body = body;
        return HttpResponse{
            200,
            R"({"message":{"content":"FINAL: captured"},"eval_count":2,"prompt_eval_count":3})"};
    }

    Result<HttpResponse> get(
        const std::string& url,
        const std::map<std::string, std::string>& headers,
        int timeout_seconds) override {
        (void)url;
        (void)headers;
        (void)timeout_seconds;
        return fail<HttpResponse>("GET not expected in this test");
    }

    std::string last_url;
    std::string last_body;
};

void test_json() {
    auto parsed = Json::parse(R"({"a":1,"b":["x",true]})");
    CHECK(parsed.has_value());
    CHECK(parsed->at("a").as_number() == 1);
    CHECK(parsed->at("b").at(0).as_string() == "x");
    CHECK(parsed->dump().find("\"a\"") != std::string::npos);
    CHECK(!json_backend_name().empty());
}

void test_calculator() {
    CalculatorTool tool;
    NativeEnvironment env(std::filesystem::temp_directory_path() / "oop_agent_test_calc");
    DefaultHttpClient http;
    ToolExecutionContext context{env, http};
    Json args = Json::object();
    args["expression"] = "15 * (17 + 3)";
    auto result = tool.execute(args, context);
    CHECK(result.success);
    CHECK(result.output == "300");
}

void test_tool_registry_factory() {
    ToolRegistry registry;
    registry.register_factory("calculator", [] {
        return std::make_unique<CalculatorTool>();
    });
    CHECK(registry.has_tool("calculator"));

    NativeEnvironment env(std::filesystem::temp_directory_path() / "oop_agent_test_factory");
    DefaultHttpClient http;
    ToolExecutionContext context{env, http};
    Json args = Json::object();
    args["expression"] = "6 * 7";
    const auto result = registry.execute("calculator", args, context);
    CHECK(result.success);
    CHECK(result.output == "42");
}

void test_file_tools() {
    const auto root = std::filesystem::temp_directory_path() / "oop_agent_test_files";
    NativeEnvironment env(root);
    CHECK(env.prepare().has_value());
    DefaultHttpClient http;
    ToolExecutionContext context{env, http};
    WriteFileTool writer;
    ReadFileTool reader;
    Json write = Json::object();
    write["path"] = "notes/result.txt";
    write["content"] = "hello oop";
    CHECK(writer.execute(write, context).success);
    Json read = Json::object();
    read["path"] = "notes/result.txt";
    auto result = reader.execute(read, context);
    CHECK(result.success);
    CHECK(result.output == "hello oop");

    CHECK(!env.resolve_inside_workspace("../escape.txt").has_value());
    CHECK(!env.resolve_inside_workspace(root.parent_path().string()).has_value());
}

void test_base64_file_reader() {
    const auto path = std::filesystem::temp_directory_path() / "oop_agent_test_image.bin";
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.put(static_cast<char>(0xFF));
        out.put(static_cast<char>(0xD8));
        out.put(static_cast<char>(0x00));
    }
    auto encoded = read_file_base64(path);
    CHECK(encoded.has_value());
    CHECK(*encoded == "/9gA");
    std::filesystem::remove(path);
}

void test_ollama_multimodal_request() {
    auto http = std::make_shared<CapturingHttpClient>();
    OllamaClient client("http://localhost:11434/", http);
    ChatRequest request;
    request.options.model = "gemma4:e2b";
    request.options.temperature = 0.1;
    request.options.max_tokens = 64;
    request.messages.push_back(ChatMessage{"user", "describe this", {"AQ=="}});
    auto response = client.chat(request);
    CHECK(response.has_value());
    CHECK(response->content == "FINAL: captured");
    CHECK(http->last_url == "http://localhost:11434/api/chat");
    CHECK(http->last_body.find("\"images\":[\"AQ==\"]") != std::string::npos);
    CHECK(http->last_body.find("\"stream\":false") != std::string::npos);
}

void test_memory_isolation() {
    const auto root = std::filesystem::temp_directory_path() / "oop_agent_test_memory";
    std::filesystem::remove_all(root);
    NativeEnvironment first(root / "first");
    NativeEnvironment second(root / "second");
    CHECK(first.prepare().has_value());
    CHECK(second.prepare().has_value());
    DefaultHttpClient http;
    ToolExecutionContext first_context{first, http};
    ToolExecutionContext second_context{second, http};
    MemorySaveTool saver;
    MemorySearchTool searcher;

    Json save = Json::object();
    save["text"] = "workspace one secret";
    save["tags"] = "isolation";
    CHECK(saver.execute(save, first_context).success);

    Json query = Json::object();
    query["query"] = "workspace one";
    query["limit"] = 5;
    CHECK(searcher.execute(query, first_context).output.find("workspace one secret") != std::string::npos);
    CHECK(searcher.execute(query, second_context).output == "No memories found");
    std::filesystem::remove_all(root);
}

void test_loop_detector() {
    LoopDetector detector({2, 3, 2, 3});
    CHECK(detector.observe_action("A").severity == LoopSeverity::None);
    CHECK(detector.observe_action("A").severity == LoopSeverity::Warning);
    CHECK(detector.observe_action("A").severity == LoopSeverity::Critical);

    LoopDetector ping_pong({3, 4, 2, 3});
    ping_pong.observe_action("A");
    ping_pong.observe_action("B");
    ping_pong.observe_action("A");
    CHECK(ping_pong.observe_action("B").severity == LoopSeverity::Warning);
    ping_pong.observe_action("A");
    CHECK(ping_pong.observe_action("B").severity == LoopSeverity::Critical);
}

void test_agent_loop_warning_and_reset() {
    const auto root = std::filesystem::temp_directory_path() / "oop_agent_test_loop_integration";
    std::filesystem::remove_all(root);
    NativeEnvironment env(root);
    CHECK(env.prepare().has_value());
    DefaultHttpClient http;
    ToolExecutionContext context{env, http};

    ToolRegistry registry;
    registry.register_tool(std::make_unique<CalculatorTool>());
    SkillLoader skills(root / "unused_skills");

    const std::string repeated_action =
        R"(ACTION: {"tool":"calculator","args":{"expression":"2+2"}})";
    ScriptedLLMClient llm({
        repeated_action,
        repeated_action,
        repeated_action,
        repeated_action});
    AgentLoop loop(llm, registry, skills, context);

    AgentRunConfig config;
    config.max_steps = 3;
    const auto first = loop.run("calculate repeatedly", config);
    CHECK(!first.success);
    CHECK(first.trajectory.steps.size() == 3);
    CHECK(first.trajectory.steps[0].tool_result == "4");
    CHECK(first.trajectory.steps[1].tool_result.find("LOOP WARNING:") != std::string::npos);
    CHECK(first.trajectory.steps[1].tool_result.find("4") != std::string::npos);
    CHECK(first.trajectory.steps[2].tool_result.find("LOOP CRITICAL:") != std::string::npos);
    CHECK(first.stop_reason.find("same action repeated 3 times") != std::string::npos);

    // The fourth scripted action starts a separate run. It must be treated as
    // the first occurrence, proving that run() cleared the previous history.
    config.max_steps = 1;
    const auto second = loop.run("calculate in a new run", config);
    CHECK(second.trajectory.steps.size() == 1);
    CHECK(second.trajectory.steps[0].tool_result == "4");
    CHECK(second.stop_reason == "max_steps reached");
    std::filesystem::remove_all(root);
}

void test_skill_loader() {
    const auto dir = std::filesystem::temp_directory_path() / "oop_agent_test_skills";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "math.md") << "# Math\nUse calculator for arithmetic.";
    SkillLoader loader(dir);
    CHECK(loader.load().has_value());
    const auto selected = loader.select_for_task("please calculate 2+2");
    CHECK(!selected.empty());
    CHECK(loader.build_prompt(selected).find("calculator") != std::string::npos);
}

void test_cpp26_preview_optional_range() {
    std::optional<int> value = 7;
    int sum = 0;
    for (const int item : cpp26_preview::one_or_empty(value)) {
        sum += item;
    }
    CHECK(sum == 7);

    value.reset();
    for (const int item : cpp26_preview::one_or_empty(value)) {
        sum += item;
    }
    CHECK(sum == 7);
}

void test_evaluator_trajectory_contract() {
    const auto root = std::filesystem::temp_directory_path() / "oop_agent_test_contract";
    std::filesystem::remove_all(root);
    NativeEnvironment env(root);
    CHECK(env.prepare().has_value());
    DefaultHttpClient http;
    ToolExecutionContext context{env, http};

    WriteFileTool writer;
    Json write = Json::object();
    write["path"] = "safe/result.txt";
    write["content"] = "PASS";
    CHECK(writer.execute(write, context).success);

    Trajectory trajectory;
    trajectory.success = true;
    trajectory.final_answer = "PASS";
    Json write_args = Json::object();
    write_args["path"] = "safe/result.txt";
    Json read_args = Json::object();
    read_args["path"] = "safe/result.txt";
    trajectory.steps = {
        StepRecord{0, "", {"tool_call", "write_file", write_args}, "Wrote PASS", 1, 0},
        StepRecord{1, "", {"tool_call", "read_file", read_args}, "PASS", 1, 0},
        StepRecord{2, "", {"done", "", Json::object()}, "Done", 1, 0}};

    Json task = Json::object();
    task["expected_files"] = Json::array();
    Json file_check = Json::object();
    file_check["path"] = "safe/result.txt";
    file_check["contains"] = "PASS";
    file_check["contains_all"] = Json::array();
    file_check["contains_all"].push_back("PASS");
    file_check["exact"] = "PASS";
    file_check["not_contains"] = "FAIL";
    task["expected_files"].push_back(file_check);
    task["expected_keywords"] = Json::array();
    task["expected_keywords"].push_back("PASS");
    task["required_tool_order"] = Json::array();
    task["required_tool_order"].push_back("write_file");
    task["required_tool_order"].push_back("read_file");
    task["min_tool_calls"] = 2;
    task["max_tool_calls"] = 2;
    task["min_steps"] = 3;
    task["expected_observations"] = Json::array();
    task["expected_observations"].push_back("Wrote PASS");
    task["expected_tool_events"] = Json::array();
    Json expected_event = Json::object();
    expected_event["tool"] = "write_file";
    expected_event["args_contains"] = "safe/result.txt";
    expected_event["result_contains"] = "Wrote PASS";
    task["expected_tool_events"].push_back(expected_event);
    Json read_event = Json::object();
    read_event["tool"] = "read_file";
    read_event["args_contains"] = "safe/result.txt";
    read_event["result_contains"] = "PASS";
    // Reverse the event contract on purpose. "any" accepts independent event
    // listing order, while ordered_event_pairs still protects causality.
    Json any_order_events = Json::array();
    any_order_events.push_back(read_event);
    any_order_events.push_back(expected_event);
    task["expected_tool_events"] = std::move(any_order_events);
    task["expected_tool_event_order"] = "any";
    task["ordered_event_pairs"] = Json::array();
    Json ordered_pair = Json::object();
    ordered_pair["first"] = expected_event;
    ordered_pair["then"] = read_event;
    task["ordered_event_pairs"].push_back(ordered_pair);
    task["forbidden_paths"] = Json::array();
    task["forbidden_paths"].push_back("../escape.txt");

    FunctionalEvaluator evaluator;
    CHECK(evaluator.evaluate(task, trajectory, env).success);

    task["max_tool_calls"] = 1;
    CHECK(!evaluator.evaluate(task, trajectory, env).success);
    task["max_tool_calls"] = 2;

    auto wrong_order = trajectory;
    std::swap(wrong_order.steps[0], wrong_order.steps[1]);
    CHECK(!evaluator.evaluate(task, wrong_order, env).success);

    task["required_tool_order"] = Json::array();
    task["required_tool_order"].push_back("calculator");
    CHECK(!evaluator.evaluate(task, trajectory, env).success);

    task["required_tool_order"] = Json::array();
    task["required_tool_order"].push_back("write_file");
    task["required_tool_order"].push_back("read_file");
    task["expected_tool_events"][0]["args_contains"] = "wrong/path.txt";
    CHECK(!evaluator.evaluate(task, trajectory, env).success);

    task["expected_tool_events"][0]["args_contains"] = "safe/result.txt";
    write["content"] = "PASS FAIL";
    CHECK(writer.execute(write, context).success);
    CHECK(!evaluator.evaluate(task, trajectory, env).success);
    std::filesystem::remove_all(root);
}

#include "agent/message_queue.h"
#include "agent/multi_agent.h"
#include "core/vector_math.h"
#include "tools/gui_tools.h"
#include "tools/screenshot_tool.h"
#include "tools/vector_memory_tools.h"

void test_vector_math() {
    std::vector<double> v1 = {1.0, 2.0, 3.0};
    std::vector<double> v2 = {1.0, 2.0, 3.0};
    std::vector<double> v3 = {-1.0, -2.0, -3.0};
    std::vector<double> v_ortho1 = {1.0, 0.0};
    std::vector<double> v_ortho2 = {0.0, 1.0};
    CHECK(std::abs(cosine_similarity(v1, v2) - 1.0) < 1e-6);
    CHECK(std::abs(cosine_similarity(v1, v3) - (-1.0)) < 1e-6);
    CHECK(std::abs(cosine_similarity(v_ortho1, v_ortho2) - 0.0) < 1e-6);
}

void test_vector_memory() {
    NativeEnvironment env(std::filesystem::temp_directory_path() / "oop_agent_test_vector_mem");
    CHECK(env.prepare().has_value());
    DefaultHttpClient http;
    ToolExecutionContext context{env, http};

    VectorMemorySaveTool saver;
    VectorMemorySearchTool searcher;

    Json save = Json::object();
    save["text"] = "C++ OOP AI Agent Framework project";
    save["tags"] = "cpp,agent";
    CHECK(saver.execute(save, context).success);

    Json search = Json::object();
    search["query"] = "AI Agent project";
    search["limit"] = 5;
    auto res = searcher.execute(search, context);
    CHECK(res.success);
    CHECK(res.output.find("Cosine Similarity") != std::string::npos);
}

void test_gui_tools() {
    NativeEnvironment env(std::filesystem::temp_directory_path() / "oop_agent_test_gui");
    CHECK(env.prepare().has_value());
    DefaultHttpClient http;
    ToolExecutionContext context{env, http};

    ScreenshotTool screenshot;
    ClickTool click;
    TypeTextTool type_text;
    KeyPressTool key_press;

    CHECK(screenshot.execute(Json::object(), context).success);
    Json click_args = Json::object();
    click_args["x"] = 100;
    click_args["y"] = 200;
    CHECK(click.execute(click_args, context).output.find("100, 200") != std::string::npos);

    Json type_args = Json::object();
    type_args["text"] = "hello";
    CHECK(type_text.execute(type_args, context).output.find("hello") != std::string::npos);

    Json key_args = Json::object();
    key_args["key"] = "Enter";
    CHECK(key_press.execute(key_args, context).output.find("Enter") != std::string::npos);
}

void test_multi_agent_queue() {
    ThreadSafeMessageQueue<std::string> queue;
    queue.push("msg1");
    queue.push("msg2");
    CHECK(queue.size() == 2);
    CHECK(queue.try_pop() == "msg1");
    CHECK(queue.wait_and_pop() == "msg2");
    CHECK(queue.empty());

    // Concurrent multi-threaded producer-consumer test
    std::thread producer([&queue]() {
        for (int i = 0; i < 5; ++i) {
            queue.push("subtask_msg_" + std::to_string(i));
        }
    });
    producer.join();
    CHECK(queue.size() == 5);
}

int main() {
    test_json();
    test_calculator();
    test_tool_registry_factory();
    test_file_tools();
    test_base64_file_reader();
    test_ollama_multimodal_request();
    test_memory_isolation();
    test_loop_detector();
    test_agent_loop_warning_and_reset();
    test_skill_loader();
    test_cpp26_preview_optional_range();
    test_evaluator_trajectory_contract();
    test_vector_math();
    test_vector_memory();
    test_gui_tools();
    test_multi_agent_queue();
    std::cout << "All core tests passed\n";
    return 0;
}
