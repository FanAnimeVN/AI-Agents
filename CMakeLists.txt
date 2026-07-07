cmake_minimum_required(VERSION 3.22)

project(OopAgentFramework LANGUAGES CXX)

option(OOP_EXPERIMENTAL_CPP26 "Build with the compiler's experimental C++26 dialect" OFF)

if(OOP_EXPERIMENTAL_CPP26)
    set(CMAKE_CXX_STANDARD 26)
else()
    set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OOP_ENABLE_CURL "Enable real HTTP calls through libcurl when available" ON)
option(OOP_ENABLE_SQLITE "Enable persistent memory through SQLite when available" ON)

add_library(oop_agent_core
    src/agent/agent_loop.cpp
    src/agent/loop_detector.cpp
    src/agent/skill_loader.cpp
    src/client/ollama_client.cpp
    src/client/scripted_llm_client.cpp
    src/core/base64.cpp
    src/core/json.cpp
    src/core/json_library_probe.cpp
    src/core/string_utils.cpp
    src/environment/environment.cpp
    src/harness/evaluator.cpp
    src/harness/harness_runner.cpp
    src/harness/trajectory.cpp
    src/http/http_client.cpp
    src/agent/multi_agent.cpp
    src/tools/calculator_tool.cpp
    src/tools/exec_tool.cpp
    src/tools/subagent_tool.cpp
    src/tools/file_tools.cpp
    src/tools/gui_tools.cpp
    src/tools/memory_tools.cpp
    src/tools/screenshot_tool.cpp
    src/tools/vector_memory_tools.cpp
    src/tools/extra_tools.cpp
    src/tools/tool_registry.cpp
    src/tools/web_search_tool.cpp
)

target_include_directories(oop_agent_core PUBLIC src)

if(MSVC)
    target_compile_options(oop_agent_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(oop_agent_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

find_package(Threads REQUIRED)
target_link_libraries(oop_agent_core PUBLIC Threads::Threads)
if(WIN32)
    target_link_libraries(oop_agent_core PUBLIC gdi32 user32)
endif()

if(OOP_ENABLE_SQLITE)
    find_package(SQLite3 QUIET)
    if(SQLite3_FOUND)
        target_compile_definitions(oop_agent_core PUBLIC OOP_HAVE_SQLITE=1)
        target_link_libraries(oop_agent_core PUBLIC SQLite::SQLite3)
    else()
        message(WARNING "SQLite3 was not found. Memory tools will use the in-memory fallback.")
    endif()
endif()

if(OOP_ENABLE_CURL)
    find_package(CURL QUIET)
    if(CURL_FOUND)
        target_compile_definitions(oop_agent_core PUBLIC OOP_HAVE_CURL=1)
        target_link_libraries(oop_agent_core PUBLIC CURL::libcurl)
    else()
        message(WARNING "libcurl was not found. Ollama and web_search will report a clear runtime error.")
    endif()
endif()

find_package(nlohmann_json QUIET)
if(nlohmann_json_FOUND)
    target_compile_definitions(oop_agent_core PUBLIC OOP_HAVE_NLOHMANN_JSON=1)
    target_link_libraries(oop_agent_core PUBLIC nlohmann_json::nlohmann_json)
else()
    message(WARNING "nlohmann_json was not found. The project will use its compact fallback JSON parser.")
endif()

add_executable(oop_agent src/main.cpp)
target_link_libraries(oop_agent PRIVATE oop_agent_core)

add_executable(oop_agent_tests tests/core_tests.cpp)
target_link_libraries(oop_agent_tests PRIVATE oop_agent_core)

enable_testing()
add_test(NAME core_tests COMMAND oop_agent_tests)
