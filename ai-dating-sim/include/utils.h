#pragma once

#include <optional>
#include <string>
#include <filesystem>

#include <nlohmann/json.hpp>

namespace ai::utils {

// 지정한 경로의 파일을 문자열로 읽어옵니다. 실패하면 빈 문자열을 반환합니다.
std::string readFile(const std::string& path);
// 문자열을 파일로 저장합니다. 상위 디렉터리가 없으면 생성합니다.
bool writeFile(const std::string& path, const std::string& content);

// JSON 파일을 로드합니다. 실패하면 std::nullopt를 반환합니다.
std::optional<nlohmann::json> loadJson(const std::string& path);
// JSON 데이터를 파일로 저장합니다. 들여쓰기 4칸으로 출력합니다.
bool saveJson(const std::string& path, const nlohmann::json& data);

// 문자열 양끝 공백을 제거한 결과를 반환합니다.
std::string trim(const std::string& text);
// 플레이어 입력에서 긍정/부정 키워드를 찾아 호감도 변화를 계산합니다.
int calculateAffectionDelta(const std::string& playerInput);

// 다양한 후보 경로를 탐색해 data 파일을 찾습니다.
std::optional<std::filesystem::path> findDataFile(const std::string& filename);

} // namespace ai::utils
