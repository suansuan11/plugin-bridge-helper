#include "bridge/ws_publisher.h"

#include "logging/logger.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <sstream>
#endif

#include <regex>
#include <string>
#include <utility>

namespace plugin_bridge {

namespace {

struct ParsedWsUrl {
  std::string host;
  int port = 80;
  std::string path = "/";
};

bool ParseWsUrl(const std::string& url, ParsedWsUrl& parsed) {
  const std::regex pattern(R"(ws://([^/:]+)(?::([0-9]+))?(/.*)?)");
  std::smatch match;
  if (!std::regex_match(url, match, pattern)) return false;
  parsed.host = match[1].str();
  parsed.port = match[2].matched ? std::stoi(match[2].str()) : 80;
  parsed.path = match[3].matched && !match[3].str().empty() ? match[3].str() : "/";
  return true;
}

#ifdef _WIN32
constexpr std::uintptr_t kInvalidSocketValue = static_cast<std::uintptr_t>(INVALID_SOCKET);

std::string WsaErrorText(const std::string& action) {
  return action + " failed; error=" + std::to_string(WSAGetLastError());
}

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

std::string Trim(const std::string& value) {
  const auto start = value.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  const auto end = value.find_last_not_of(" \t\r\n");
  return value.substr(start, end - start + 1);
}

std::string HeaderValue(const std::string& request, const std::string& headerName) {
  const std::string wanted = ToLower(headerName);
  std::istringstream lines(request);
  std::string line;
  while (std::getline(lines, line)) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;
    if (ToLower(Trim(line.substr(0, colon))) == wanted) {
      return Trim(line.substr(colon + 1));
    }
  }
  return "";
}

std::string Base64Encode(const unsigned char* data, size_t size) {
  constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string output;
  output.reserve(((size + 2) / 3) * 4);
  for (size_t index = 0; index < size; index += 3) {
    const unsigned int octetA = data[index];
    const unsigned int octetB = index + 1 < size ? data[index + 1] : 0;
    const unsigned int octetC = index + 2 < size ? data[index + 2] : 0;
    const unsigned int triple = (octetA << 16) | (octetB << 8) | octetC;
    output.push_back(kAlphabet[(triple >> 18) & 0x3F]);
    output.push_back(kAlphabet[(triple >> 12) & 0x3F]);
    output.push_back(index + 1 < size ? kAlphabet[(triple >> 6) & 0x3F] : '=');
    output.push_back(index + 2 < size ? kAlphabet[triple & 0x3F] : '=');
  }
  return output;
}

std::string MakeWebSocketAccept(const std::string& clientKey) {
  constexpr char kWebSocketGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  const std::string source = clientKey + kWebSocketGuid;
  BCRYPT_ALG_HANDLE algorithm = nullptr;
  std::array<unsigned char, 20> hash{};
  if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0) < 0) return "";
  const auto status = BCryptHash(algorithm, nullptr, 0,
                                 reinterpret_cast<PUCHAR>(const_cast<char*>(source.data())),
                                 static_cast<ULONG>(source.size()), hash.data(),
                                 static_cast<ULONG>(hash.size()));
  BCryptCloseAlgorithmProvider(algorithm, 0);
  if (status < 0) return "";
  return Base64Encode(hash.data(), hash.size());
}

bool SendAll(SOCKET socket, const char* data, size_t size) {
  size_t sent = 0;
  while (sent < size) {
    const int result = send(socket, data + sent, static_cast<int>(size - sent), 0);
    if (result <= 0) return false;
    sent += static_cast<size_t>(result);
  }
  return true;
}

bool SendTextFrame(SOCKET socket, const std::string& payload) {
  std::string frame;
  frame.push_back(static_cast<char>(0x81));
  if (payload.size() < 126) {
    frame.push_back(static_cast<char>(payload.size()));
  } else if (payload.size() <= 0xFFFF) {
    frame.push_back(static_cast<char>(126));
    frame.push_back(static_cast<char>((payload.size() >> 8) & 0xFF));
    frame.push_back(static_cast<char>(payload.size() & 0xFF));
  } else {
    frame.push_back(static_cast<char>(127));
    for (int shift = 56; shift >= 0; shift -= 8) {
      frame.push_back(static_cast<char>((static_cast<unsigned long long>(payload.size()) >> shift) & 0xFF));
    }
  }
  frame.append(payload);
  return SendAll(socket, frame.data(), frame.size());
}

bool SendHandshake(SOCKET socket, const std::string& request) {
  const std::string key = HeaderValue(request, "Sec-WebSocket-Key");
  if (key.empty()) return false;
  const std::string accept = MakeWebSocketAccept(key);
  if (accept.empty()) return false;
  const std::string response =
      "HTTP/1.1 101 Switching Protocols\r\n"
      "Upgrade: websocket\r\n"
      "Connection: Upgrade\r\n"
      "Sec-WebSocket-Accept: " +
      accept + "\r\n\r\n";
  return SendAll(socket, response.data(), response.size());
}

bool ReadHttpUpgradeRequest(SOCKET socket, std::string& request) {
  std::array<char, 2048> buffer{};
  while (request.find("\r\n\r\n") == std::string::npos && request.size() < 16384) {
    const int bytes = recv(socket, buffer.data(), static_cast<int>(buffer.size()), 0);
    if (bytes <= 0) return false;
    request.append(buffer.data(), static_cast<size_t>(bytes));
  }
  return request.find("\r\n\r\n") != std::string::npos;
}

#endif

}  // namespace

WsPublisher::WsPublisher(BridgeConfig config, Logger& logger) : config_(std::move(config)), logger_(logger) {
#ifdef _WIN32
  listenSocket_.store(kInvalidSocketValue);
#endif
}

WsPublisher::~WsPublisher() {
  Close();
}

bool WsPublisher::Connect() {
#ifndef _WIN32
  logger_.Warn("WebSocket publisher is only implemented on Windows; running in dry mode");
  return false;
#else
  Close();
  ParsedWsUrl parsed;
  if (!ParseWsUrl(config_.url, parsed)) {
    logger_.Error("Invalid WebSocket URL: " + config_.url);
    return false;
  }

  WSADATA wsaData{};
  if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
    logger_.Warn(WsaErrorText("WSAStartup"));
    return false;
  }

  SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (listenSocket == INVALID_SOCKET) {
    WSACleanup();
    logger_.Warn(WsaErrorText("socket"));
    return false;
  }

  BOOL reuse = TRUE;
  setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<unsigned short>(parsed.port));
  if (parsed.host == "localhost") {
    parsed.host = "127.0.0.1";
  }
  if (inet_pton(AF_INET, parsed.host.c_str(), &address.sin_addr) != 1) {
    closesocket(listenSocket);
    WSACleanup();
    logger_.Error("Bridge WebSocket server only supports IPv4 host addresses; url=" + config_.url);
    return false;
  }

  if (bind(listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
    closesocket(listenSocket);
    WSACleanup();
    logger_.Warn(WsaErrorText("bind " + config_.url));
    return false;
  }
  if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
    closesocket(listenSocket);
    WSACleanup();
    logger_.Warn(WsaErrorText("listen " + config_.url));
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);
    wsaStarted_ = true;
    listenSocket_.store(static_cast<std::uintptr_t>(listenSocket));
    running_ = true;
    acceptThread_ = std::thread(&WsPublisher::AcceptLoop, this);
  }

  logger_.Info("Bridge WebSocket server listening on " + config_.url);
  return true;
#endif
}

void WsPublisher::Close() {
#ifdef _WIN32
  std::thread acceptThread;
  std::vector<std::uintptr_t> clients;
  SOCKET listenSocket = INVALID_SOCKET;
  bool cleanupWsa = false;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
    listenSocket = static_cast<SOCKET>(listenSocket_.load());
    listenSocket_.store(kInvalidSocketValue);
    clients.swap(clients_);
    acceptThread.swap(acceptThread_);
    cleanupWsa = wsaStarted_;
    wsaStarted_ = false;
  }

  if (listenSocket != INVALID_SOCKET) {
    shutdown(listenSocket, SD_BOTH);
    closesocket(listenSocket);
  }
  for (const auto clientValue : clients) {
    const SOCKET client = static_cast<SOCKET>(clientValue);
    shutdown(client, SD_BOTH);
    closesocket(client);
  }
  if (acceptThread.joinable()) {
    acceptThread.join();
  }
  if (cleanupWsa) {
    WSACleanup();
  }
#endif
}

bool WsPublisher::Publish(const std::string& json) {
#ifndef _WIN32
  logger_.Info("Dry publish: " + json);
  return false;
#else
  if (!running_) {
    logger_.Warn("Bridge WebSocket server is not running; cannot publish event");
    return false;
  }

  std::vector<std::uintptr_t> clients;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    clients = clients_;
  }

  if (clients.empty()) {
    logger_.Warn("Bridge WebSocket server has no connected overlay clients; event queued nowhere");
    return false;
  }

  bool delivered = false;
  for (const auto clientValue : clients) {
    const SOCKET client = static_cast<SOCKET>(clientValue);
    if (SendTextFrame(client, json)) {
      delivered = true;
      continue;
    }

    logger_.Warn(WsaErrorText("send bridge websocket frame"));
    {
      std::lock_guard<std::mutex> lock(mutex_);
      clients_.erase(std::remove(clients_.begin(), clients_.end(), clientValue), clients_.end());
    }
    shutdown(client, SD_BOTH);
    closesocket(client);
  }

  return delivered;
#endif
}

#ifdef _WIN32
void WsPublisher::AcceptLoop() {
  while (running_) {
    const SOCKET listenSocket = static_cast<SOCKET>(listenSocket_.load());
    SOCKET client = accept(listenSocket, nullptr, nullptr);
    if (client == INVALID_SOCKET) {
      if (running_) {
        logger_.Warn(WsaErrorText("accept bridge websocket client"));
      }
      continue;
    }

    DWORD timeoutMs = 3000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));

    std::string request;
    if (!ReadHttpUpgradeRequest(client, request) || !SendHandshake(client, request)) {
      logger_.Warn("Rejected invalid bridge websocket handshake");
      shutdown(client, SD_BOTH);
      closesocket(client);
      continue;
    }

    {
      std::lock_guard<std::mutex> lock(mutex_);
      clients_.push_back(static_cast<std::uintptr_t>(client));
    }
    logger_.Info("Overlay Bridge Receiver connected to helper WebSocket server");
  }
}
#endif

}  // namespace plugin_bridge
