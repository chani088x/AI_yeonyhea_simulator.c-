#pragma once

#include <string>

namespace ai {

class OllamaClient {
public:
    OllamaClient(const std::string& endpoint, const std::string& model);
    ~OllamaClient();

    std::string generateReply(
        const std::string& persona,
        const std::string& conversationHistory,
        const std::string& playerInput,
        std::string& outError) const;

private:
    std::string buildPrompt(
        const std::string& persona,
        const std::string& conversationHistory,
        const std::string& playerInput) const;

    std::string endpoint_;
    std::string model_;
};

} // namespace ai
