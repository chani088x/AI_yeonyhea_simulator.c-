#pragma once

#include <string>

#include <nlohmann/json.hpp>

namespace ai {

// 캐릭터의 상태와 페르소나, 호감도, 마지막 대화를 관리하는 클래스입니다.
class Character {
public:
    // 이름과 페르소나 텍스트를 받아 캐릭터를 초기화합니다. 초기 호감도는 50으로 설정됩니다.
    Character(std::string name, std::string persona);

    // 캐릭터 이름을 반환합니다.
    const std::string& getName() const noexcept;
    // 캐릭터의 페르소나 원본 텍스트를 반환합니다.
    const std::string& getPersona() const noexcept;

    // 현재 호감도 값을 확인합니다.
    int getAffection() const noexcept;
    // 호감도를 지정한 값으로 설정합니다.
    void setAffection(int value) noexcept;
    // 호감도에 증감을 적용합니다.
   void adjustAffection(int delta) noexcept;

    // 마지막 대화 기록을 반환합니다.
    const std::string& getLastDialogue() const noexcept;
    // 마지막 대화 기록을 새 문자열로 교체합니다.
    void setLastDialogue(std::string dialogue);

    // 저장된 JSON 데이터를 읽어 호감도와 마지막 대화를 복원합니다.
    void applySaveData(const nlohmann::json& data);
    // 현재 상태를 JSON으로 직렬화합니다.
    nlohmann::json toJson() const;

    // 페르소나의 첫 줄을 이용해 초기 인사말을 생성합니다.
    std::string initialGreeting() const;

private:
    std::string name_;
    std::string persona_;
    int affection_;
    std::string lastDialogue_;
};

} // namespace ai
