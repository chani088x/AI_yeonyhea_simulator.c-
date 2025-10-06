#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace ai {

class Character {
public:
    Character(std::string name, std::string persona);

    const std::string& getName() const noexcept;
    const std::string& getPersona() const noexcept;

    int getAffection() const noexcept;
    void setAffection(int value) noexcept;
    void adjustAffection(int delta) noexcept;

    const std::string& getLastDialogue() const noexcept;
    void setLastDialogue(std::string dialogue);

    void applySaveData(const nlohmann::json& data);
    nlohmann::json toJson() const;

    std::string initialGreeting() const;

private:
    std::string name_;
    std::string persona_;
    int affection_;
    std::string lastDialogue_;
};

} // namespace ai
