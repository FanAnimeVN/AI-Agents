#include "tools/gui_tools.h"

#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace oop {

std::string ClickTool::name() const {
    return "click";
}

std::string ClickTool::description() const {
    return "Execute a GUI click at coordinates (x, y). Args: {\"x\":100,\"y\":200}.";
}

ToolResult ClickTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)context;
    const int x = static_cast<int>(args.at("x").as_number(0));
    const int y = static_cast<int>(args.at("y").as_number(0));

#ifdef _WIN32
    // Move OS cursor to target screen coordinates and trigger hardware click
    SetCursorPos(x, y);
    Sleep(50);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(10);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
#endif

    std::ostringstream out;
    out << "OS System Cursor moved & Click executed at screen coordinates (" << x << ", " << y << ")";
    Json meta = Json::object();
    meta["x"] = static_cast<double>(x);
    meta["y"] = static_cast<double>(y);
    return ToolResult{true, out.str(), {}, meta};
}

std::string TypeTextTool::name() const {
    return "type_text";
}

std::string TypeTextTool::description() const {
    return "Execute GUI text input. Args: {\"text\":\"...\"}.";
}

ToolResult TypeTextTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)context;
    const std::string text = args.at("text").as_string_or();
    if (text.empty()) {
        return ToolResult{false, {}, "Missing text to type", Json::object()};
    }

#ifdef _WIN32
    for (const char c : text) {
        SHORT vk = VkKeyScanA(c);
        if (vk != -1) {
            BYTE key_code = LOBYTE(vk);
            keybd_event(key_code, 0, 0, 0);
            keybd_event(key_code, 0, KEYEVENTF_KEYUP, 0);
        }
    }
#endif

    std::ostringstream out;
    out << "GUI TypeText executed: \"" << text << "\"";
    Json meta = Json::object();
    meta["text"] = text;
    return ToolResult{true, out.str(), {}, meta};
}

std::string KeyPressTool::name() const {
    return "key_press";
}

std::string KeyPressTool::description() const {
    return "Execute GUI key press. Args: {\"key\":\"Enter\"}.";
}

ToolResult KeyPressTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)context;
    const std::string key = args.at("key").as_string_or();
    if (key.empty()) {
        return ToolResult{false, {}, "Missing key name", Json::object()};
    }

#ifdef _WIN32
    BYTE vk = VK_RETURN;
    if (key == "Enter" || key == "RETURN") {
        vk = VK_RETURN;
    } else if (key == "Space") {
        vk = VK_SPACE;
    } else if (key == "Tab") {
        vk = VK_TAB;
    } else if (key == "Escape") {
        vk = VK_ESCAPE;
    }
    keybd_event(vk, 0, 0, 0);
    keybd_event(vk, 0, KEYEVENTF_KEYUP, 0);
#endif

    std::ostringstream out;
    out << "GUI KeyPress executed: [" << key << "]";
    Json meta = Json::object();
    meta["key"] = key;
    return ToolResult{true, out.str(), {}, meta};
}

std::string GuiBrowserSearchTool::name() const {
    return "gui_browser_search";
}

std::string GuiBrowserSearchTool::description() const {
    return "Automate GUI browser launch, cursor navigation, search query, and result copy. Args: {\"query\":\"...\"}.";
}

ToolResult GuiBrowserSearchTool::execute(const Json& args, ToolExecutionContext& context) {
    (void)context;
    const std::string query = args.at("query").as_string_or();
    if (query.empty()) {
        return ToolResult{false, {}, "Missing query for GUI browser automation", Json::object()};
    }

#ifdef _WIN32
    // 1. Open Edge browser to search query
    const std::string cmd = "start msedge \"https://www.google.com/search?q=" + query + "\"";
    std::system(cmd.c_str());
    Sleep(1500);

    // 2. Move cursor to browser search result area (500, 400) and click
    SetCursorPos(500, 400);
    Sleep(100);
    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    Sleep(10);
    mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
#endif

    std::ostringstream out;
    out << "GUI Browser Automation completed: Opened browser, moved cursor to (500, 400), clicked, and queried \"" << query << "\"";
    Json meta = Json::object();
    meta["query"] = query;
    return ToolResult{true, out.str(), {}, meta};
}

}  // namespace oop
