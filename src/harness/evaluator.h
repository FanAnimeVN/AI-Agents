#pragma once

#include "core/json.h"
#include "core/result.h"
#include "environment/environment.h"
#include "harness/trajectory.h"

#include <memory>
#include <string>
#include <vector>

namespace oop {

struct EvaluationResult {
    bool success = false;
    double score = 0.0;
    std::string message;
    Json details = Json::object();
};

class Evaluator {
public:
    virtual ~Evaluator() = default;
    [[nodiscard]] virtual std::string name() const = 0;
    virtual EvaluationResult evaluate(
        const Json& task,
        const Trajectory& trajectory,
        const Environment& environment) = 0;
};

class KeywordEvaluator final : public Evaluator {
public:
    [[nodiscard]] std::string name() const override;
    EvaluationResult evaluate(
        const Json& task,
        const Trajectory& trajectory,
        const Environment& environment) override;
};

class FunctionalEvaluator final : public Evaluator {
public:
    [[nodiscard]] std::string name() const override;
    EvaluationResult evaluate(
        const Json& task,
        const Trajectory& trajectory,
        const Environment& environment) override;
};

class VLMEvaluator final : public Evaluator {
public:
    [[nodiscard]] std::string name() const override;
    EvaluationResult evaluate(
        const Json& task,
        const Trajectory& trajectory,
        const Environment& environment) override;
};

std::unique_ptr<Evaluator> make_evaluator(const std::string& type);

}  // namespace oop
