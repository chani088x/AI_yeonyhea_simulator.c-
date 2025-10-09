#include "character.h"

#include <algorithm>
#include <sstream>

#include <nlohmann/json.hpp>

namespace ai {

// 캐릭터 생성자는 이름과 페르소나를 저장하고 기본 호감도를 50으로 설정합니다.
Character::Character(std::string name, std::string persona)
    : name_(std::move(name)), persona_(std::move(persona)), affection_(50) {}

// 캐릭터 이름을 읽어옵니다.
const std::string& Character::getName() const noexcept { return name_; }

// 페르소나 텍스트를 반환합니다.
const std::string& Character::getPersona() const noexcept { return persona_; }

// 현재 호감도 값을 확인합니다.
int Character::getAffection() const noexcept { return affection_; }

// 호감도를 특정 값으로 덮어씁니다.
void Character::setAffection(int value) noexcept { affection_ = value; }

// 호감도에 증감을 적용합니다.
void Character::adjustAffection(int delta) noexcept { affection_ += delta; }

// 가장 최근 대화 기록을 반환합니다.
const std::string& Character::getLastDialogue() const noexcept { return lastDialogue_; }

// 마지막 대화 기록을 새로운 문자열로 저장합니다.
void Character::setLastDialogue(std::string dialogue) { lastDialogue_ = std::move(dialogue); }

// 저장된 JSON에서 호감도와 마지막 대화를 추출해 캐릭터 상태를 복원합니다.
void Character::applySaveData(const nlohmann::json& data) {
    if (data.contains("affection") && data["affection"].is_number_integer()) {
        affection_ = data["affection"].get<int>();
    }
    if (data.contains("last_dialogue") && data["last_dialogue"].is_string()) {
        lastDialogue_ = data["last_dialogue"].get<std::string>();
    }
}

nlohmann::json Character::toJson() const {
    // 현재 상태를 JSON 객체로 구성합니다.
    nlohmann::json data;
    data["affection"] = affection_;
    data["last_dialogue"] = lastDialogue_;
    return data;
}

// 페르소나 첫 줄을 기반으로 초기 인사말을 만들고, 없다면 기본 문구를 돌려줍니다.
std::string Character::initialGreeting() const {
    std::istringstream stream(persona_);
    std::string firstLine;
    std::getline(stream, firstLine);
    if (firstLine.empty()) {
        return name_ + ": 만나서 반가워!";
    }
    return firstLine;
}

} // namespace ai
