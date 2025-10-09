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

// 실행 파일이 위치한 디렉터리를 추정해 반환합니다.
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

// 후보 목록에 중복 없이 경로를 추가합니다.
void addCandidate(std::vector<std::filesystem::path>& candidates, const std::filesystem::path& path) {
    if (path.empty()) {
        return;
    }
    if (std::find(candidates.begin(), candidates.end(), path) == candidates.end()) {
        candidates.push_back(path);
    }
}

// 기준 경로에서 상위 디렉터리를 따라가며 data 경로 후보를 확장합니다.
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

// 파일을 바이너리 모드로 열어 전체 내용을 문자열로 돌려줍니다.
std::string readFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

// 파일을 저장하기 전에 필요한 디렉터리를 만들고 내용을 기록합니다.
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

// JSON 파일을 열어 파싱한 뒤 결과를 반환합니다.
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

// JSON 데이터를 보기 좋게 들여쓰기 하여 저장합니다.
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

// 양쪽 끝 공백 문자를 제거한 문자열을 생성합니다.
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

// 입력된 문장에서 감정 키워드를 찾아 호감도 증감치를 계산합니다.
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

// 여러 경로 후보를 순회하며 실제 파일이 존재하면 경로를 반환합니다.
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
