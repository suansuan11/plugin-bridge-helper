#include "bridge/ws_publisher.h"

#include "logging/logger.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

#include <regex>
#include <utility>

namespace plugin_bridge {

struct ParsedWsUrl {
  std::string host;
  int port = 80;
  std::string path = "/";
};

static bool ParseWsUrl(const std::string& url, ParsedWsUrl& parsed) {
  const std::regex pattern(R"(ws://([^/:]+)(?::([0-9]+))?(/.*)?)");
  std::smatch match;
  if (!std::regex_match(url, match, pattern)) return false;
  parsed.host = match[1].str();
  parsed.port = match[2].matched ? std::stoi(match[2].str()) : 80;
  parsed.path = match[3].matched && !match[3].str().empty() ? match[3].str() : "/";
  return true;
}

WsPublisher::WsPublisher(BridgeConfig config, Logger& logger) : config_(std::move(config)), logger_(logger) {}

WsPublisher::~WsPublisher() {
  Close();
}

bool WsPublisher::Connect() {
  std::lock_guard<std::mutex> lock(mutex_);
#ifndef _WIN32
  logger_.Warn("WebSocket publisher is only implemented with WinHTTP on Windows; running in dry mode");
  return false;
#else
  Close();
  ParsedWsUrl parsed;
  if (!ParseWsUrl(config_.url, parsed)) {
    logger_.Error("Invalid WebSocket URL: " + config_.url);
    return false;
  }

  std::wstring host(parsed.host.begin(), parsed.host.end());
  std::wstring path(parsed.path.begin(), parsed.path.end());
  session_ = WinHttpOpen(L"plugin-bridge-helper/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                         WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session_) return false;
  connection_ = WinHttpConnect(static_cast<HINTERNET>(session_), host.c_str(), static_cast<INTERNET_PORT>(parsed.port), 0);
  if (!connection_) return false;
  request_ = WinHttpOpenRequest(static_cast<HINTERNET>(connection_), L"GET", path.c_str(), nullptr,
                                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0);
  if (!request_) return false;
  if (!WinHttpSetOption(static_cast<HINTERNET>(request_), WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0)) return false;
  if (!WinHttpSendRequest(static_cast<HINTERNET>(request_), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                          WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) return false;
  if (!WinHttpReceiveResponse(static_cast<HINTERNET>(request_), nullptr)) return false;
  websocket_ = WinHttpWebSocketCompleteUpgrade(static_cast<HINTERNET>(request_), 0);
  if (!websocket_) return false;
  WinHttpCloseHandle(static_cast<HINTERNET>(request_));
  request_ = nullptr;
  logger_.Info("Connected to overlay bridge: " + config_.url);
  return true;
#endif
}

void WsPublisher::Close() {
#ifdef _WIN32
  if (websocket_) {
    WinHttpWebSocketClose(static_cast<HINTERNET>(websocket_), WINHTTP_WEB_SOCKET_SUCCESS_CLOSE_STATUS, nullptr, 0);
    WinHttpCloseHandle(static_cast<HINTERNET>(websocket_));
    websocket_ = nullptr;
  }
  if (request_) {
    WinHttpCloseHandle(static_cast<HINTERNET>(request_));
    request_ = nullptr;
  }
  if (connection_) {
    WinHttpCloseHandle(static_cast<HINTERNET>(connection_));
    connection_ = nullptr;
  }
  if (session_) {
    WinHttpCloseHandle(static_cast<HINTERNET>(session_));
    session_ = nullptr;
  }
#endif
}

bool WsPublisher::Publish(const std::string& json) {
#ifndef _WIN32
  logger_.Info("Dry publish: " + json);
  return false;
#else
  if (!websocket_ && !Connect()) return false;
  std::lock_guard<std::mutex> lock(mutex_);
  if (!websocket_) return false;
  const auto result = WinHttpWebSocketSend(static_cast<HINTERNET>(websocket_), WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE,
                                          const_cast<char*>(json.data()), static_cast<DWORD>(json.size()));
  if (result != NO_ERROR) {
    logger_.Warn("WebSocket send failed; reconnecting");
    Close();
    return false;
  }
  return true;
#endif
}

}  // namespace plugin_bridge
