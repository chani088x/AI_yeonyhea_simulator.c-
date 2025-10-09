#include "ollama_client.h"

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <sstream>
#include <stdexcept>
#include <string>

namespace ai {

namespace {

// libcurl이 수신한 데이터를 문자열에 누적하기 위한 콜백입니다.
size_t writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    if (userdata == nullptr) {
        return 0;
    }
    auto* buffer = static_cast<std::string*>(userdata);
    buffer->append(ptr, size * nmemb);
    return size * nmemb;
}

// libcurl 전역 초기화를 한 번만 수행합니다.
bool ensureCurlInitialized(std::string& outError) {
    static bool initialized = false;
    static bool initAttempted = false;

    if (!initAttempted) {
        initAttempted = true;
        if (curl_global_init(CURL_GLOBAL_DEFAULT) == 0) {
            initialized = true;
        } else {
            outError = "libcurl 초기화에 실패했습니다.";
        }
    }

    if (!initialized && outError.empty()) {
        outError = "libcurl을 사용할 수 없습니다.";
    }
    return initialized;
}

} // namespace

OllamaClient::OllamaClient(const std::string& endpoint, const std::string& model)
    : endpoint_(endpoint), model_(model) {}

std::string OllamaClient::generateReply(const std::string& persona,
                                        const std::string& conversationHistory,
                                        const std::string& playerInput,
                                        std::string& outError) const {
    outError.clear();

    if (!ensureCurlInitialized(outError)) {
        return {};
    }

    CURL* curl = curl_easy_init();
    if (!curl) {
        outError = "libcurl 핸들을 생성할 수 없습니다.";
        return {};
    }

    std::string responseBuffer;
    std::string prompt = buildPrompt(persona, conversationHistory, playerInput);

    nlohmann::json payload = {
        {"model", model_},
        {"prompt", prompt},
        {"stream", false}
    };

    std::string payloadStr = payload.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, endpoint_.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payloadStr.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);

    CURLcode result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        outError = std::string("HTTP 요청 실패: ") + curl_easy_strerror(result);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return {};
    }

    long httpStatus = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpStatus);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (httpStatus >= 400) {
        outError = "서버가 오류 상태(" + std::to_string(httpStatus) + ")를 반환했습니다.";
        return {};
    }

    if (responseBuffer.empty()) {
        outError = "Ollama 응답이 비어 있습니다.";
        return {};
    }

    try {
        // stream=false 설정으로 단일 JSON 객체가 반환됩니다.
        auto jsonResponse = nlohmann::json::parse(responseBuffer);
        if (jsonResponse.contains("error") && jsonResponse["error"].is_string()) {
            outError = jsonResponse["error"].get<std::string>();
            return {};
        }
        if (jsonResponse.contains("response") && jsonResponse["response"].is_string()) {
            return jsonResponse["response"].get<std::string>();
        }
        if (jsonResponse.contains("message") && jsonResponse["message"].is_string()) {
            return jsonResponse["message"].get<std::string>();
        }
        outError = "예상치 못한 응답 형식입니다.";
    } catch (const std::exception& ex) {
        outError = std::string("JSON 파싱 실패: ") + ex.what();
    }

    return {};
}

std::string OllamaClient::buildPrompt(const std::string& persona,
                                      const std::string& conversationHistory,
                                      const std::string& playerInput) const {
    std::ostringstream oss;
    oss << "너는 가상의 여자친구 '" << "사야" << "'로서 오빠에게 다정하게 답해야 해.\n";
    oss << "사야의 페르소나:\n" << persona << "\n\n";
    if (!conversationHistory.empty()) {
        oss << "지금까지의 대화:\n" << conversationHistory << "\n";
    }
    oss << "오빠: " << playerInput << "\n";
    oss << "사야:";
    return oss.str();
}

} // namespace ai
