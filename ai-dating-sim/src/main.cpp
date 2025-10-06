#include <iostream>
#include <string>

#include <nlohmann/json.hpp>

#include "character.h"
#include "ollama_client.h"
#include "utils.h"

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    const std::string personaPath = "data/persona.txt";
    const std::string savePath = "data/save.json";

    std::string persona = ai::utils::readFile(personaPath);
    if (persona.empty()) {
        std::cerr << "[에러] 페르소나 파일을 불러오지 못했습니다: " << personaPath << "\n";
        return 1;
    }

    ai::Character saya("사야", persona);

    if (auto saveData = ai::utils::loadJson(savePath)) {
        saya.applySaveData(*saveData);
    }

    std::string conversationHistory = saya.getLastDialogue();

    std::cout << saya.initialGreeting() << "\n";
    if (!conversationHistory.empty()) {
        std::cout << "[지난 대화]\n" << conversationHistory << "\n";
    }

    ai::OllamaClient client("http://localhost:11434/api/generate", "llama3");

    std::string input;
    while (true) {
        std::cout << "현재 호감도: " << saya.getAffection() << "\n";
        std::cout << "오빠: ";
        if (!std::getline(std::cin, input)) {
            break;
        }

        input = ai::utils::trim(input);
        if (input == "exit") {
            break;
        }
        if (input.empty()) {
            continue;
        }

        std::string error;
        std::string reply = client.generateReply(persona, conversationHistory, input, error);
        if (reply.empty()) {
            if (!error.empty()) {
                std::cerr << "[에러] " << error << "\n";
            } else {
                std::cerr << "[에러] Ollama 응답이 비어 있습니다.\n";
            }
            continue;
        }

        int delta = ai::utils::calculateAffectionDelta(input);
        saya.adjustAffection(delta);

        if (!conversationHistory.empty()) {
            conversationHistory += "\n";
        }
        conversationHistory += "오빠: " + input + "\n사야: " + reply;
        saya.setLastDialogue(conversationHistory);

        if (!ai::utils::saveJson(savePath, saya.toJson())) {
            std::cerr << "[경고] 저장 파일을 업데이트할 수 없습니다." << "\n";
        }

        std::cout << "사야: " << reply << "\n";
        if (delta != 0) {
            std::cout << "(호감도 " << (delta > 0 ? "+" : "") << delta << ")\n";
        }
    }

    if (!ai::utils::saveJson(savePath, saya.toJson())) {
        std::cerr << "[경고] 게임 종료 시 저장에 실패했습니다." << "\n";
    }

    std::cout << "게임을 종료합니다. 사야가 기다릴게요!" << "\n";
    return 0;
}
