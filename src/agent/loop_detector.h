#pragma once

#include <deque>
#include <optional>
#include <string>

namespace oop {

enum class LoopSeverity {
    None,
    Warning,
    Critical
};

struct LoopSignal {
    LoopSeverity severity = LoopSeverity::None;
    std::string reason;
};

struct LoopDetectorConfig {
    int repeat_warning_threshold = 2;
    int repeat_critical_threshold = 3;
    int ping_pong_warning_threshold = 2;
    int ping_pong_critical_threshold = 3;
};

class LoopDetector {
public:
    explicit LoopDetector(LoopDetectorConfig config = {});

    LoopSignal observe_action(const std::string& signature);
    void reset();

private:
    LoopDetectorConfig config_;
    std::deque<std::string> history_;
};

}  // namespace oop
