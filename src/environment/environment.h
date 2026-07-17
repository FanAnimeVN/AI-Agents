#pragma once

#include "core/result.h"

#include <filesystem>
#include <string>

namespace oop {

class Environment {
public:
    virtual ~Environment() = default;
    [[nodiscard]] virtual std::filesystem::path workspace_root() const = 0;
    virtual Result<void> prepare() = 0;
    virtual Result<std::filesystem::path> resolve_inside_workspace(const std::string& relative_path) const = 0;
};

class NativeEnvironment final : public Environment {
public:
    explicit NativeEnvironment(std::filesystem::path workspace_root);

    [[nodiscard]] std::filesystem::path workspace_root() const override;
    Result<void> prepare() override;
    Result<std::filesystem::path> resolve_inside_workspace(const std::string& relative_path) const override;

private:
    std::filesystem::path root_;
};

class SandboxEnvironment final : public Environment {
public:
    explicit SandboxEnvironment(std::filesystem::path workspace_root);

    [[nodiscard]] std::filesystem::path workspace_root() const override;
    Result<void> prepare() override;
    Result<std::filesystem::path> resolve_inside_workspace(const std::string& relative_path) const override;

private:
    NativeEnvironment native_;
};

}  // namespace oop
