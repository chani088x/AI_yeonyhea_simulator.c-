#include "ollama_client.h"

#include <optional>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "httplib.h"

namespace {

struct ParsedEndpoint {
    std::string scheme;
    std::string host;
    int port;
    std::string path;
};

std::optional<ParsedEndpoint> parseEndpoint(const std::string& endpoint, std::string& outError) {
    ParsedEndpoint parsed;
    parsed.port = 0;

    const std::string httpPrefix = "http://";
    const std::string httpsPrefix = "https://";

    std::string remainder;
    if (endpoint.rfind(httpPrefix, 0) == 0) {
        parsed.scheme = "http";
        remainder = endpoint.substr(httpPrefix.size());
        parsed.port = 80;
    } else if (endpoint.rfind(httpsPrefix, 0) == 0) {
        parsed.scheme = "https";
        remainder = endpoint.substr(httpsPrefix.size());
        parsed.port = 443;
    } else {
        outError = "지원하지 않는 프로토콜입니다. http:// 또는 https:// 형식을 사용하세요.";
        return std::nullopt;
    }

    auto slashPos = remainder.find('/');
    std::string hostPort = slashPos == std::string::npos ? remainder : remainder.substr(0, slashPos);
    parsed.path = slashPos == std::string::npos ? "/" : remainder.substr(slashPos);
    if (parsed.path.empty()) {
        parsed.path = "/";
    }

    if (hostPort.empty()) {
        outError = "엔드포인트에 호스트가 없습니다.";
        return std::nullopt;
    }

    auto colonPos = hostPort.find(':');
    if (colonPos == std::string::npos) {
        parsed.host = hostPort;
    } else {
        parsed.host = hostPort.substr(0, colonPos);
        auto portStr = hostPort.substr(colonPos + 1);
        if (portStr.empty()) {
            outError = "포트 번호가 비어 있습니다.";
            return std::nullopt;
        }
        try {
            int port = std::stoi(portStr);
            if (port <= 0 || port > 65535) {
                outError = "포트 번호가 유효하지 않습니다.";
                return std::nullopt;
            }
            parsed.port = port;
        } catch (const std::exception&) {
            outError = "포트 번호를 해석할 수 없습니다.";
            return std::nullopt;
        }
    }

    return parsed;
}

} // namespace

namespace ai {

OllamaClient::OllamaClient(const std::string& endpoint, const std::string& model)
    : endpoint_(endpoint), model_(model) {}

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

    auto parsed = parseEndpoint(endpoint_, outError);
    if (!parsed) {
        return {};
    }

    auto performRequest = [&](auto& cli) -> std::string {
        cli.set_connection_timeout(5, 0);
        cli.set_read_timeout(30, 0);
        cli.set_write_timeout(30, 0);

        std::string prompt = buildPrompt(persona, conversationHistory, playerInput);

        nlohmann::json payload;
        payload["model"] = model_;
        payload["prompt"] = prompt;
        payload["stream"] = false;

        std::string payloadStr = payload.dump();

        httplib::Headers headers = {
            {"Content-Type", "application/json"},
            {"Accept", "application/json"}
        };

        auto response = cli.Post(parsed->path.c_str(), headers, payloadStr, "application/json");
        if (!response) {
            outError = "Ollama API 요청에 실패했습니다.";
            return std::string{};
        }

        if (response->status < 200 || response->status >= 300) {
            outError = "Ollama API가 오류 상태를 반환했습니다: " + std::to_string(response->status);
            return std::string{};
        }

        try {
            auto jsonResponse = nlohmann::json::parse(response->body);
            if (jsonResponse.contains("response")) {
                return jsonResponse["response"].template get<std::string>();
            }
            if (jsonResponse.contains("message")) {
                return jsonResponse["message"].template get<std::string>();
            }
            outError = "예상치 못한 응답 형식";
            return std::string{};
        } catch (const std::exception& ex) {
            outError = std::string("JSON 파싱 실패: ") + ex.what();
            return std::string{};
        }
    };

    if (parsed->scheme == "https") {
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
        httplib::SSLClient cli(parsed->host, parsed->port);
        return performRequest(cli);
#else
        outError = "HTTPS 요청을 수행하려면 OpenSSL 지원이 포함된 cpp-httplib를 사용해야 합니다.";
        return {};
#endif
    }

    httplib::Client cli(parsed->host, parsed->port);
    return performRequest(cli);
}

} // namespace ai
