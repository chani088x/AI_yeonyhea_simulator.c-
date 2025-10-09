#include "ollama_client.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <optional>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

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

#ifdef _WIN32
class WinsockInitializer {
public:
    explicit WinsockInitializer(std::string& outError) {
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (result != 0) {
            outError = "WSAStartup 실패: " + std::to_string(result);
            return;
        }
        initialized_ = true;
    }

    ~WinsockInitializer() {
        if (initialized_) {
            WSACleanup();
        }
    }

    bool initialized() const { return initialized_; }

private:
    bool initialized_ = false;
};
#endif

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
#endif

void closeSocket(SocketHandle socket) {
#ifdef _WIN32
    if (socket != INVALID_SOCKET) {
        closesocket(socket);
    }
#else
    if (socket >= 0) {
        close(socket);
    }
#endif
}

bool setSocketTimeouts(SocketHandle socket, std::string& outError) {
#ifdef _WIN32
    DWORD timeoutMs = 30000; // 30초
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs)) == SOCKET_ERROR) {
        outError = "SO_RCVTIMEO 설정 실패";
        return false;
    }
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs)) == SOCKET_ERROR) {
        outError = "SO_SNDTIMEO 설정 실패";
        return false;
    }
#else
    timeval tv{};
    tv.tv_sec = 30;
    tv.tv_usec = 0;
    if (setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        outError = "SO_RCVTIMEO 설정 실패";
        return false;
    }
    if (setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        outError = "SO_SNDTIMEO 설정 실패";
        return false;
    }
#endif
    return true;
}

SocketHandle connectToEndpoint(const ParsedEndpoint& parsed, std::string& outError) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* result = nullptr;
    std::string portStr = std::to_string(parsed.port);
    int rc = getaddrinfo(parsed.host.c_str(), portStr.c_str(), &hints, &result);
    if (rc != 0) {
#ifdef _WIN32
        outError = "호스트를 해석할 수 없습니다: " + std::string(gai_strerrorA(rc));
#else
        outError = "호스트를 해석할 수 없습니다: " + std::string(gai_strerror(rc));
#endif
        return kInvalidSocket;
    }

    SocketHandle sock = kInvalidSocket;
    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next) {
        SocketHandle candidate = static_cast<SocketHandle>(socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol));
        if (candidate == kInvalidSocket) {
            continue;
        }

        if (connect(candidate, ptr->ai_addr, static_cast<int>(ptr->ai_addrlen)) == 0) {
            sock = candidate;
            break;
        }

        closeSocket(candidate);
    }

    freeaddrinfo(result);

    if (sock == kInvalidSocket) {
        outError = "Ollama 서버에 연결할 수 없습니다.";
        return kInvalidSocket;
    }

    if (!setSocketTimeouts(sock, outError)) {
        closeSocket(sock);
        return kInvalidSocket;
    }

    return sock;
}

bool sendAll(SocketHandle socket, const std::string& data, std::string& outError) {
    size_t totalSent = 0;
    while (totalSent < data.size()) {
        const char* buffer = data.data() + totalSent;
        size_t remaining = data.size() - totalSent;
#ifdef _WIN32
        int sent = send(socket, buffer, static_cast<int>(remaining), 0);
        if (sent == SOCKET_ERROR) {
            outError = "데이터 전송 중 오류가 발생했습니다.";
            return false;
        }
#else
        ssize_t sent = send(socket, buffer, remaining, 0);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            outError = "데이터 전송 중 오류가 발생했습니다.";
            return false;
        }
#endif
        if (sent == 0) {
            outError = "데이터를 모두 전송하지 못했습니다.";
            return false;
        }
        totalSent += static_cast<size_t>(sent);
    }
    return true;
}

std::optional<std::string> receiveAll(SocketHandle socket, std::string& outError) {
    std::string result;
    char buffer[4096];
    while (true) {
#ifdef _WIN32
        int received = recv(socket, buffer, static_cast<int>(sizeof(buffer)), 0);
        if (received == 0) {
            break;
        }
        if (received == SOCKET_ERROR) {
            int errorCode = WSAGetLastError();
            if (errorCode == WSAETIMEDOUT) {
                outError = "응답 수신이 시간 초과되었습니다.";
            } else {
                outError = "데이터 수신 중 오류가 발생했습니다.";
            }
            return std::nullopt;
        }
#else
        ssize_t received = recv(socket, buffer, sizeof(buffer), 0);
        if (received == 0) {
            break;
        }
        if (received < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                outError = "응답 수신이 시간 초과되었습니다.";
            } else {
                outError = "데이터 수신 중 오류가 발생했습니다.";
            }
            return std::nullopt;
        }
#endif
        result.append(buffer, static_cast<size_t>(received));
    }
    return result;
}

std::string toLower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return str;
}

std::string trim(const std::string& str) {
    const auto begin = str.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(begin, end - begin + 1);
}

std::optional<std::string> decodeChunkedBody(const std::string& body, std::string& outError) {
    std::string decoded;
    size_t pos = 0;
    while (pos < body.size()) {
        size_t lineEnd = body.find("\r\n", pos);
        if (lineEnd == std::string::npos) {
            outError = "chunked 인코딩 헤더를 해석할 수 없습니다.";
            return std::nullopt;
        }
        std::string sizeLine = body.substr(pos, lineEnd - pos);
        auto semicolonPos = sizeLine.find(';');
        if (semicolonPos != std::string::npos) {
            sizeLine = sizeLine.substr(0, semicolonPos);
        }
        size_t chunkSize = 0;
        try {
            chunkSize = std::stoul(sizeLine, nullptr, 16);
        } catch (const std::exception&) {
            outError = "chunked 인코딩 크기를 해석할 수 없습니다.";
            return std::nullopt;
        }
        pos = lineEnd + 2;
        if (chunkSize == 0) {
            return decoded;
        }
        if (pos + chunkSize > body.size()) {
            outError = "chunked 본문 크기가 일치하지 않습니다.";
            return std::nullopt;
        }
        decoded.append(body, pos, chunkSize);
        pos += chunkSize;
        if (pos + 2 > body.size() || body.substr(pos, 2) != "\r\n") {
            outError = "chunked 본문 형식이 잘못되었습니다.";
            return std::nullopt;
        }
        pos += 2;
    }
    outError = "chunked 본문이 예상보다 짧습니다.";
    return std::nullopt;
}

std::optional<std::string> extractBody(const std::string& response, std::string& outError) {
    auto headerEnd = response.find("\r\n\r\n");
    if (headerEnd == std::string::npos) {
        outError = "HTTP 헤더를 구분할 수 없습니다.";
        return std::nullopt;
    }

    std::string header = response.substr(0, headerEnd);
    std::string body = response.substr(headerEnd + 4);

    std::istringstream headerStream(header);
    std::string statusLine;
    if (!std::getline(headerStream, statusLine)) {
        outError = "HTTP 상태 줄을 읽을 수 없습니다.";
        return std::nullopt;
    }
    if (!statusLine.empty() && statusLine.back() == '\r') {
        statusLine.pop_back();
    }

    std::istringstream statusStream(statusLine);
    std::string httpVersion;
    int statusCode = 0;
    if (!(statusStream >> httpVersion >> statusCode)) {
        outError = "HTTP 상태 줄 형식이 잘못되었습니다.";
        return std::nullopt;
    }

    if (statusCode < 200 || statusCode >= 300) {
        outError = "Ollama API가 오류 상태를 반환했습니다: " + std::to_string(statusCode);
        return std::nullopt;
    }

    bool isChunked = false;
    size_t contentLength = 0;
    std::string line;
    while (std::getline(headerStream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        auto lowerLine = toLower(line);
        if (lowerLine.rfind("transfer-encoding:", 0) == 0 && lowerLine.find("chunked") != std::string::npos) {
            isChunked = true;
        } else if (lowerLine.rfind("content-length:", 0) == 0) {
            auto value = trim(line.substr(std::string("content-length:").size()));
            try {
                contentLength = static_cast<size_t>(std::stoull(value));
            } catch (const std::exception&) {
                outError = "Content-Length 값을 해석할 수 없습니다.";
                return std::nullopt;
            }
        }
    }

    if (isChunked) {
        return decodeChunkedBody(body, outError);
    }

    if (contentLength != 0 && body.size() < contentLength) {
        outError = "HTTP 본문이 예상보다 짧습니다.";
        return std::nullopt;
    }

    if (contentLength != 0) {
        return body.substr(0, contentLength);
    }

    return body;
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

    if (parsed->scheme == "https") {
        outError = "현재 구현은 HTTPS를 지원하지 않습니다. HTTP 엔드포인트를 사용하세요.";
        return {};
    }

#ifdef _WIN32
    WinsockInitializer winsock(outError);
    if (!winsock.initialized()) {
        return {};
    }
#endif

    SocketHandle socket = connectToEndpoint(*parsed, outError);
    if (socket == kInvalidSocket) {
        return {};
    }

    std::string prompt = buildPrompt(persona, conversationHistory, playerInput);

    nlohmann::json payload;
    payload["model"] = model_;
    payload["prompt"] = prompt;
    payload["stream"] = false;

    std::string payloadStr = payload.dump();

    std::ostringstream request;
    request << "POST " << parsed->path << " HTTP/1.1\r\n";
    request << "Host: " << parsed->host;
    if (!(parsed->port == 80 || parsed->port == 443)) {
        request << ':' << parsed->port;
    }
    request << "\r\n";
    request << "Content-Type: application/json\r\n";
    request << "Accept: application/json\r\n";
    request << "Connection: close\r\n";
    request << "Content-Length: " << payloadStr.size() << "\r\n\r\n";
    request << payloadStr;

    std::string requestStr = request.str();
    bool sent = sendAll(socket, requestStr, outError);
    if (!sent) {
        closeSocket(socket);
        return {};
    }

    auto response = receiveAll(socket, outError);
    closeSocket(socket);

    if (!response) {
        return {};
    }

    auto body = extractBody(*response, outError);
    if (!body) {
        return {};
    }

    try {
        auto jsonResponse = nlohmann::json::parse(*body);
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
