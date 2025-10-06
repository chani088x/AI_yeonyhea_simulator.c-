#include "utils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ai::utils {

namespace {

std::optional<std::filesystem::path> executableDirectory() {
#ifdef __linux__
    std::error_code ec;
    auto exe = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        return exe.parent_path();
    }
#elif defined(_WIN32)
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (len > 0 && len < buffer.size()) {
        buffer.resize(len);
        return std::filesystem::path(buffer).parent_path();
    }
#endif
    return std::nullopt;
}

void addCandidate(std::vector<std::filesystem::path>& candidates, const std::filesystem::path& path) {
    if (path.empty()) {
        return;
    }
    if (std::find(candidates.begin(), candidates.end(), path) == candidates.end()) {
        candidates.push_back(path);
    }
}

void expandWithHierarchy(std::vector<std::filesystem::path>& candidates, const std::filesystem::path& base, const std::string& filename) {
    namespace fs = std::filesystem;
    fs::path current = base;
    for (int depth = 0; depth < 4 && !current.empty(); ++depth) {
        addCandidate(candidates, current / filename);
        addCandidate(candidates, current / "data" / filename);
        addCandidate(candidates, current / "ai-dating-sim" / "data" / filename);
        current = current.parent_path();
    }
}

} // namespace

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
    namespace fs = std::filesystem;
    fs::path target(path);
    if (target.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
    }
    std::ofstream file(target, std::ios::binary);
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
    namespace fs = std::filesystem;
    fs::path target(path);
    if (target.has_parent_path()) {
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
    }
    std::ofstream file(target);
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

std::optional<std::filesystem::path> findDataFile(const std::string& filename) {
    namespace fs = std::filesystem;
    std::vector<fs::path> candidates;

    expandWithHierarchy(candidates, fs::current_path(), filename);

    if (auto exeDir = executableDirectory()) {
        expandWithHierarchy(candidates, *exeDir, filename);
    }

    for (const auto& candidate : candidates) {
        std::error_code ec;
        if (fs::is_regular_file(candidate, ec)) {
            return candidate;
        }
    }

    return std::nullopt;
}

} // namespace ai::utils
