#include "utils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace ai::utils {

std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool writeFile(const std::string& path, const std::string& content) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    file << content;
    return static_cast<bool>(file);
}

std::optional<nlohmann::json> loadJson(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return std::nullopt;
    }
    try {
        nlohmann::json data;
        file >> data;
        return data;
    } catch (...) {
        return std::nullopt;
    }
}

bool saveJson(const std::string& path, const nlohmann::json& data) {
    std::ofstream file(path);
    if (!file) {
        return false;
    }
    file << data.dump(4);
    return static_cast<bool>(file);
}

std::string trim(const std::string& text) {
    auto start = text.begin();
    while (start != text.end() && std::isspace(static_cast<unsigned char>(*start))) {
        ++start;
    }
    auto end = text.end();
    while (end != start && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(start, end);
}

int calculateAffectionDelta(const std::string& playerInput) {
    int delta = 0;
    if (playerInput.find("좋아") != std::string::npos || playerInput.find("사랑") != std::string::npos) {
        delta += 5;
    }
    if (playerInput.find("싫어") != std::string::npos || playerInput.find("미워") != std::string::npos) {
        delta -= 5;
    }
    return delta;
}

} // namespace ai::utils
