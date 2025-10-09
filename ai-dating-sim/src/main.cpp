#include <cstdlib>
#include <iostream>
#include <string>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "character.h"
#include "ollama_client.h"
#include "utils.h"

// 콘솔에서 사야와 대화를 주고받는 메인 루프입니다.
int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    // data 폴더에서 페르소나 파일 경로를 탐색합니다.
    auto personaPath = ai::utils::findDataFile("persona.txt");
    if (!personaPath) {
        std::cerr << "[에러] 페르소나 파일을 찾을 수 없습니다. 프로젝트의 data 폴더가 존재하는지 확인하세요.\n";
        return 1;
    }

    const std::filesystem::path dataDirectory = personaPath->parent_path();
    const std::filesystem::path savePath = dataDirectory / "save.json";

    // 페르소나 내용을 읽어옵니다.
    std::string persona = ai::utils::readFile(personaPath->string());
    if (persona.empty()) {
        std::cerr << "[에러] 페르소나 파일을 불러오지 못했습니다: " << personaPath->string() << "\n";
        return 1;
    }

    // 캐릭터 인스턴스를 생성하고 저장 데이터가 있으면 적용합니다.
    ai::Character saya("사야", persona);

    if (auto saveData = ai::utils::loadJson(savePath.string())) {
        saya.applySaveData(*saveData);
    }

    // 이전 플레이에서 이어받은 대화 기록을 가져옵니다.
    std::string conversationHistory = saya.getLastDialogue();

    // 첫 인사와 지난 대화를 출력합니다.
    std::cout << saya.initialGreeting() << std::endl;
    if (!conversationHistory.empty()) {
        std::cout << "[지난 대화]\n" << conversationHistory << std::endl;
    }

    // 환경 변수에서 엔드포인트와 모델 값을 확인합니다.
    const char* endpointEnv = std::getenv("OLLAMA_ENDPOINT");
    const char* modelEnv = std::getenv("OLLAMA_MODEL");

    std::string endpoint = endpointEnv ? endpointEnv : "http://localhost:11434/api/generate";
    std::string model = modelEnv ? modelEnv : "llama3:8b";

    std::cout << "[안내] Ollama 엔드포인트: " << endpoint << " (모델: " << model << ")" << std::endl;

    // Ollama API와 통신할 클라이언트를 준비합니다.
    ai::OllamaClient client(endpoint, model);

    std::string input;
    while (true) {
        // 현재 호감도 안내와 사용자 입력을 받습니다.
        std::cout << "현재 호감도: " << saya.getAffection() << std::endl;
        std::cout << "오빠: " << std::flush;
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
        // Ollama에 요청을 보내 사야의 답변을 받습니다.
        std::string reply = client.generateReply(persona, conversationHistory, input, error);
        if (reply.empty()) {
            if (!error.empty()) {
                std::cerr << "[에러] " << error << "\n";
            } else {
                std::cerr << "[에러] Ollama 응답이 비어 있습니다.\n";
            }
            continue;
        }

        // 입력에 따라 호감도 변화를 계산해 적용합니다.
        int delta = ai::utils::calculateAffectionDelta(input);
        saya.adjustAffection(delta);

        if (!conversationHistory.empty()) {
            conversationHistory += "\n";
        }
        // 현재 턴의 대화를 대화록 끝에 추가합니다.
        conversationHistory += "오빠: " + input + "\n사야: " + reply;
        saya.setLastDialogue(conversationHistory);

        // 변경된 상태를 저장 파일에 기록합니다.
        if (!ai::utils::saveJson(savePath.string(), saya.toJson())) {
            std::cerr << "[경고] 저장 파일을 업데이트할 수 없습니다." << "\n";
        }

        std::cout << "사야: " << reply << std::endl;
        if (delta != 0) {
            std::cout << "(호감도 " << (delta > 0 ? "+" : "") << delta << ")" << std::endl;
        }
    }

    // 종료 시에도 마지막 상태를 저장합니다.
    if (!ai::utils::saveJson(savePath.string(), saya.toJson())) {
        std::cerr << "[경고] 게임 종료 시 저장에 실패했습니다." << "\n";
    }

    std::cout << "게임을 종료합니다. 사야가 기다릴게요!" << std::endl;
    return 0;
}
