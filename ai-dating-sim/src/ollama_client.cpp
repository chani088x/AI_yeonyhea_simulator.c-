#include "ollama_client.h"

#include <curl/curl.h>

#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace {

size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realSize = size * nmemb;
    auto* buffer = static_cast<std::string*>(userp);
    buffer->append(static_cast<const char*>(contents), realSize);
    return realSize;
}

} // namespace

namespace ai {

OllamaClient::OllamaClient(const std::string& endpoint, const std::string& model)
    : endpoint_(endpoint), model_(model) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

OllamaClient::~OllamaClient() { curl_global_cleanup(); }

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

std::string OllamaClient::generateReply(const std::string& persona,
                                        const std::string& conversationHistory,
                                        const std::string& playerInput,
                                        std::string& outError) const {
    outError.clear();
    CURL* curl = curl_easy_init();
    if (!curl) {
        outError = "libcurl 초기화 실패";
        return {};
    }

    std::string prompt = buildPrompt(persona, conversationHistory, playerInput);

    nlohmann::json payload;
    payload["model"] = model_;
    payload["prompt"] = prompt;
    payload["stream"] = false;

    std::string payloadStr = payload.dump();

    std::string responseBuffer;

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    curl_easy_setopt(curl, CURLOPT_URL, endpoint_.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payloadStr.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(payloadStr.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        outError = curl_easy_strerror(res);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return {};
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    try {
        auto jsonResponse = nlohmann::json::parse(responseBuffer);
        if (jsonResponse.contains("response")) {
            return jsonResponse["response"].get<std::string>();
        }
        if (jsonResponse.contains("message")) {
            return jsonResponse["message"].get<std::string>();
        }
        outError = "예상치 못한 응답 형식";
        return {};
    } catch (const std::exception& ex) {
        outError = std::string("JSON 파싱 실패: ") + ex.what();
        return {};
    }
}

} // namespace ai
