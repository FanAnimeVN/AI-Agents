#include "agent/skill_loader.h"

#include "core/string_utils.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <ranges>
#include <sstream>

namespace oop {
namespace {

Result<std::string> read_file(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) {
        return fail<std::string>("Cannot open skill file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<std::string> extract_keywords(const std::string& name, const std::string& content) {
    std::map<std::string, int> freq;
    for (const auto& word : str::split_words(name + " " + content)) {
        if (word.size() >= 4) {
            ++freq[word];
        }
    }
    std::vector<std::pair<std::string, int>> ranked(freq.begin(), freq.end());
    std::ranges::sort(ranked, {}, [](const auto& item) { return -item.second; });
    std::vector<std::string> out;
    for (const auto& [word, _] : ranked | std::views::take(12)) {
        out.push_back(word);
    }
    return out;
}

int score_skill(const Skill& skill, const std::string& task_lower) {
    int score = 0;
    for (const auto& keyword : skill.keywords) {
        if (task_lower.find(keyword) != std::string::npos) {
            score += 2;
        }
    }
    if (task_lower.find(str::to_lower(skill.name)) != std::string::npos) {
        score += 4;
    }
    return score;
}

}  // namespace

SkillLoader::SkillLoader(std::filesystem::path skills_dir)
    : skills_dir_(std::move(skills_dir)) {}

Result<void> SkillLoader::load() {
    skills_.clear();
    if (!std::filesystem::exists(skills_dir_)) {
        return std::unexpected("Skills directory does not exist: " + skills_dir_.string());
    }
    for (const auto& entry : std::filesystem::directory_iterator(skills_dir_)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".md") {
            continue;
        }
        auto content = read_file(entry.path());
        if (!content) {
            return fail<void>(content.error());
        }
        const auto name = entry.path().stem().string();
        skills_.push_back(Skill{name, entry.path(), *content, extract_keywords(name, *content)});
    }
    if (skills_.empty()) {
        return std::unexpected("No .md skills found in: " + skills_dir_.string());
    }
    return {};
}

std::vector<Skill> SkillLoader::select_for_task(const std::string& task, std::size_t limit) const {
    std::vector<std::pair<int, Skill>> ranked;
    const std::string task_lower = str::to_lower(task);
    for (const auto& skill : skills_) {
        ranked.emplace_back(score_skill(skill, task_lower), skill);
    }
    std::ranges::sort(ranked, [](const auto& a, const auto& b) {
        return a.first > b.first;
    });
    std::vector<Skill> selected;
    for (const auto& [score, skill] : ranked) {
        if (selected.size() >= limit) {
            break;
        }
        if (score > 0 || selected.empty()) {
            selected.push_back(skill);
        }
    }
    return selected;
}

std::string SkillLoader::build_prompt(const std::vector<Skill>& skills) const {
    std::ostringstream out;
    out << "Relevant skills loaded from Markdown files:\n";
    for (const auto& skill : skills) {
        out << "\n<skill name=\"" << skill.name << "\">\n" << skill.content << "\n</skill>\n";
    }
    return out.str();
}

const std::vector<Skill>& SkillLoader::skills() const {
    return skills_;
}

}  // namespace oop
