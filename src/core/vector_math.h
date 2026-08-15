#pragma once

#include <cmath>
#include <numeric>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace oop {

inline double dot_product(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.size() != b.size()) {
        return 0.0;
    }
    double sum = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

inline double magnitude(const std::vector<double>& v) {
    double sum_sq = 0.0;
    for (const double val : v) {
        sum_sq += val * val;
    }
    return std::sqrt(sum_sq);
}

inline double cosine_similarity(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.empty() || b.empty() || a.size() != b.size()) {
        return 0.0;
    }
    const double mag_a = magnitude(a);
    const double mag_b = magnitude(b);
    if (mag_a == 0.0 || mag_b == 0.0) {
        return 0.0;
    }
    return dot_product(a, b) / (mag_a * mag_b);
}

inline std::vector<double> text_to_vector(std::string_view text, const std::vector<std::string>& vocab) {
    std::vector<double> vec(vocab.size(), 0.0);
    std::string lower_text;
    lower_text.reserve(text.size());
    for (const char c : text) {
        lower_text.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }

    for (std::size_t i = 0; i < vocab.size(); ++i) {
        std::string lower_word;
        lower_word.reserve(vocab[i].size());
        for (const char c : vocab[i]) {
            lower_word.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        std::size_t pos = 0;
        while ((pos = lower_text.find(lower_word, pos)) != std::string::npos) {
            vec[i] += 1.0;
            pos += lower_word.size();
        }
    }
    return vec;
}

}  // namespace oop
