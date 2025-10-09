#pragma once

#include <string>

namespace ai {

// Ollama API와 통신해 사야의 응답을 받아오는 libcurl 기반 HTTP 클라이언트입니다.
class OllamaClient {
public:
    // 엔드포인트와 모델 이름을 지정하여 클라이언트를 구성합니다.
    OllamaClient(const std::string& endpoint, const std::string& model);

    // 페르소나, 대화 기록, 플레이어 입력을 합쳐 프롬프트를 만들고 Ollama에 요청합니다.
    // 실패 시 빈 문자열을 반환하고 outError에 원인을 기록합니다.
    std::string generateReply(
        const std::string& persona,
        const std::string& conversationHistory,
        const std::string& playerInput,
        std::string& outError) const;

private:
    // Ollama에게 전달할 프롬프트 문자열을 구성합니다.
    std::string buildPrompt(
        const std::string& persona,
        const std::string& conversationHistory,
        const std::string& playerInput) const;

    std::string endpoint_;
    std::string model_;
};

} // namespace ai
