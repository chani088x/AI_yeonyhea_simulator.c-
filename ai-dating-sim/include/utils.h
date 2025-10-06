#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace ai::utils {

std::string readFile(const std::string& path);
bool writeFile(const std::string& path, const std::string& content);

std::optional<nlohmann::json> loadJson(const std::string& path);
bool saveJson(const std::string& path, const nlohmann::json& data);

std::string trim(const std::string& text);
int calculateAffectionDelta(const std::string& playerInput);

} // namespace ai::utils
