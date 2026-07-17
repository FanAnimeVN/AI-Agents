#include "environment/environment.h"

#include <system_error>

namespace oop {

NativeEnvironment::NativeEnvironment(std::filesystem::path workspace_root)
    : root_(std::filesystem::absolute(std::move(workspace_root))) {}

std::filesystem::path NativeEnvironment::workspace_root() const {
    return root_;
}

Result<void> NativeEnvironment::prepare() {
    std::error_code ec;
    std::filesystem::create_directories(root_, ec);
    if (ec) {
        return std::unexpected("Cannot create workspace: " + ec.message());
    }
    return {};
}

Result<std::filesystem::path> NativeEnvironment::resolve_inside_workspace(const std::string& relative_path) const {
    if (relative_path.empty()) {
        return fail<std::filesystem::path>("Path is empty");
    }

    const std::filesystem::path requested(relative_path);
    if (requested.is_absolute()) {
        return fail<std::filesystem::path>("Path must be relative to workspace: " + relative_path);
    }

    std::error_code ec;
    const auto canonical_root = std::filesystem::weakly_canonical(root_, ec);
    if (ec) {
        return fail<std::filesystem::path>("Cannot resolve workspace: " + ec.message());
    }

    ec.clear();
    const auto candidate = std::filesystem::weakly_canonical(canonical_root / requested, ec);
    if (ec) {
        return fail<std::filesystem::path>("Cannot resolve path: " + ec.message());
    }

    // Compare path components instead of strings. A string prefix check would
    // incorrectly accept a sibling such as <workspace>2/file.txt.
    auto root_it = canonical_root.begin();
    auto candidate_it = candidate.begin();
    for (; root_it != canonical_root.end(); ++root_it, ++candidate_it) {
        if (candidate_it == candidate.end() || *root_it != *candidate_it) {
            return fail<std::filesystem::path>("Path escapes workspace: " + relative_path);
        }
    }

    return candidate;
}

SandboxEnvironment::SandboxEnvironment(std::filesystem::path workspace_root)
    : native_(std::move(workspace_root)) {}

std::filesystem::path SandboxEnvironment::workspace_root() const {
    return native_.workspace_root();
}

Result<void> SandboxEnvironment::prepare() {
    return native_.prepare();
}

Result<std::filesystem::path> SandboxEnvironment::resolve_inside_workspace(const std::string& relative_path) const {
    return native_.resolve_inside_workspace(relative_path);
}

}  // namespace oop
