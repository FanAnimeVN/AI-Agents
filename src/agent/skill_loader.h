#pragma once

#include "core/result.h"

#include <filesystem>
#include <string>
#include <vector>

namespace oop {

struct Skill {
    std::string name;
    std::filesystem::path path;
    std::string content;
    std::vector<std::string> keywords;
};

class SkillLoader {
public:
    explicit SkillLoader(std::filesystem::path skills_dir);

    Result<void> load();
    [[nodiscard]] std::vector<Skill> select_for_task(const std::string& task, std::size_t limit = 3) const;
    [[nodiscard]] std::string build_prompt(const std::vector<Skill>& skills) const;
    [[nodiscard]] const std::vector<Skill>& skills() const;

private:
    std::filesystem::path skills_dir_;
    std::vector<Skill> skills_;
};

}  // namespace oop
