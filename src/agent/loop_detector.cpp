#include "agent/loop_detector.h"

#include <algorithm>

namespace oop {

LoopDetector::LoopDetector(LoopDetectorConfig config)
    : config_(config) {}

LoopSignal LoopDetector::observe_action(const std::string& signature) {
    history_.push_back(signature);
    while (history_.size() > 12) {
        history_.pop_front();
    }

    int repeat_count = 1;
    for (auto it = history_.rbegin() + 1; it != history_.rend(); ++it) {
        if (*it != signature) {
            break;
        }
        ++repeat_count;
    }
    if (repeat_count >= config_.repeat_critical_threshold) {
        return {LoopSeverity::Critical, "generic repeat loop: same action repeated " + std::to_string(repeat_count) + " times"};
    }
    if (repeat_count >= config_.repeat_warning_threshold) {
        return {LoopSeverity::Warning, "generic repeat warning"};
    }

    int ping_pong_pairs = 0;
    if (history_.size() >= 4) {
        auto it = history_.rbegin();
        const auto a = *it++;
        const auto b = *it++;
        if (a != b) {
            // The newest (A, B) already forms the first pair. Count matching
            // earlier pairs so A,B,A,B reaches the warning threshold of two.
            ping_pong_pairs = 1;
            while (it != history_.rend()) {
                if (*it++ != a || it == history_.rend() || *it++ != b) {
                    break;
                }
                ++ping_pong_pairs;
            }
        }
    }
    if (ping_pong_pairs >= config_.ping_pong_critical_threshold) {
        return {LoopSeverity::Critical, "ping-pong loop: alternating actions repeated"};
    }
    if (ping_pong_pairs >= config_.ping_pong_warning_threshold) {
        return {LoopSeverity::Warning, "ping-pong loop warning"};
    }
    return {};
}

void LoopDetector::reset() {
    history_.clear();
}

}  // namespace oop
