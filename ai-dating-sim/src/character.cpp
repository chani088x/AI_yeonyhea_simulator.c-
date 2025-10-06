#include "character.h"

#include <algorithm>
#include <sstream>

#include <nlohmann/json.hpp>

namespace ai {

Character::Character(std::string name, std::string persona)
    : name_(std::move(name)), persona_(std::move(persona)), affection_(50) {}

const std::string& Character::getName() const noexcept { return name_; }

const std::string& Character::getPersona() const noexcept { return persona_; }

int Character::getAffection() const noexcept { return affection_; }

void Character::setAffection(int value) noexcept { affection_ = value; }

void Character::adjustAffection(int delta) noexcept { affection_ += delta; }

const std::string& Character::getLastDialogue() const noexcept { return lastDialogue_; }

void Character::setLastDialogue(std::string dialogue) { lastDialogue_ = std::move(dialogue); }

void Character::applySaveData(const nlohmann::json& data) {
    if (data.contains("affection") && data["affection"].is_number_integer()) {
        affection_ = data["affection"].get<int>();
    }
    if (data.contains("last_dialogue") && data["last_dialogue"].is_string()) {
        lastDialogue_ = data["last_dialogue"].get<std::string>();
    }
}

nlohmann::json Character::toJson() const {
    nlohmann::json data;
    data["affection"] = affection_;
    data["last_dialogue"] = lastDialogue_;
    return data;
}

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
