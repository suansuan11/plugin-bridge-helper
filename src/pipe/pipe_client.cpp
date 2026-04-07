#include "pipe/pipe_client.h"

#include "events/official_message_parser.h"
#include "logging/logger.h"
#include "util/time_util.h"
#include "util/uuid.h"

#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <utility>

#ifdef PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK
#if __has_include(<PipeDef.h>)
#include <PipeDef.h>
#elif __has_include(<PipeSDK.h>)
#include <PipeSDK.h>
#elif __has_include(<pipesdk.h>)
#include <pipesdk.h>
#elif __has_include(<PipeSDK/PipeSDK.h>)
#include <PipeSDK/PipeSDK.h>
#else
#error "PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK is ON, but no official PipeSDK header was found. Put PipeDef.h/PipeSDK.h under the official SDK root."
#endif
#endif

namespace plugin_bridge {

namespace {

std::string MakeRequestJson(const std::string& reqId, const std::string& method, const std::string& paramsJson) {
  return "{\"type\":\"request\",\"reqId\":\"" + reqId + "\",\"method\":\"" + method + "\",\"params\":" + paramsJson + "}";
}

bool IsEscaped(const std::string& value, size_t quote) {
  size_t backslashes = 0;
  while (quote > backslashes && value[quote - backslashes - 1] == '\\') {
    ++backslashes;
  }
  return backslashes % 2 == 1;
}

std::string ExtractJsonString(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const size_t keyPos = json.find(needle);
  if (keyPos == std::string::npos) return "";
  const size_t colon = json.find(':', keyPos + needle.size());
  if (colon == std::string::npos) return "";
  size_t start = colon + 1;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) ++start;
  if (start >= json.size() || json[start] != '"') return "";
  ++start;
  for (size_t end = start; end < json.size(); ++end) {
    if (json[end] == '"' && !IsEscaped(json, end)) {
      return json.substr(start, end - start);
    }
  }
  return "";
}

std::string ExtractJsonNumberToken(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const size_t keyPos = json.find(needle);
  if (keyPos == std::string::npos) return "";
  const size_t colon = json.find(':', keyPos + needle.size());
  if (colon == std::string::npos) return "";
  size_t value = colon + 1;
  while (value < json.size() && std::isspace(static_cast<unsigned char>(json[value]))) ++value;
  const size_t numberStart = value;
  if (value < json.size() && json[value] == '-') ++value;
  while (value < json.size() && std::isdigit(static_cast<unsigned char>(json[value]))) ++value;
  return value > numberStart ? json.substr(numberStart, value - numberStart) : "";
}

bool HasNonSuccessResponseCode(const std::string& json) {
  const std::string code = ExtractJsonNumberToken(json, "code");
  return !code.empty() && code != "1";
}

}  // namespace

class MockPipeClient final : public IPipeClient {
 public:
  bool Initialize(const LaunchArgs& args, Logger& logger) override {
    logger.Info("Mock PipeClient initialized; pipeName=" + args.pipeName);
    return true;
  }

  bool Subscribe(const std::string& capability, Logger& logger) override {
    logger.Info("Mock subscribed capability: " + capability);
    if (!callback_) {
      return true;
    }

    OfficialInteractionEvent event;
    event.sourceEventId = "mock-" + capability + "-" + GenerateUuid();
    event.timestampMs = NowMillis();
    event.userId = "mock-user";
    event.nickname = "Mock User";
    event.rawJson = "{\"mock\":true,\"capability\":\"" + capability + "\"}";

    if (capability == "comment") {
      event.type = OfficialEventType::Comment;
      event.text = "Mock comment event";
      callback_(event);
    } else if (capability == "like") {
      event.type = OfficialEventType::Like;
      event.likeCount = 1;
      callback_(event);
    } else if (capability == "gift") {
      event.type = OfficialEventType::Gift;
      event.giftName = "Mock Gift";
      event.giftCount = 1;
      callback_(event);
    } else if (capability == "fans_club") {
      event.type = OfficialEventType::FansClub;
      event.fansClubLevel = 3;
      event.text = "Mock fans club event";
      callback_(event);
    } else if (capability == "follow") {
      event.type = OfficialEventType::Follow;
      event.followAction = "follow";
      callback_(event);
    } else if (capability == "total_like") {
      event.type = OfficialEventType::TotalLike;
      event.totalLikeCount = 100;
      callback_(event);
    }
    return true;
  }

  bool SendRequest(const std::string& requestJson, Logger& logger) override {
    logger.Info("Mock request: " + requestJson);
    return true;
  }

  void SetEventCallback(EventCallback callback) override {
    callback_ = std::move(callback);
  }

  bool ShouldExit() const override {
    return false;
  }

  void Shutdown(Logger& logger) override {
    logger.Info("Mock PipeClient shutdown");
  }

 private:
  EventCallback callback_;
};

class OfficialPipeClient final : public IPipeClient {
 public:
  ~OfficialPipeClient() override {
    JoinSubscribeWorker();
  }

  bool Initialize(const LaunchArgs& args, Logger& logger) override {
#ifdef PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK
    logger_ = &logger;
    activeLogger_ = &logger;
    disconnected_.store(false);
    connected_.store(false);
    openLiveDataSubscribed_ = false;

    PipeSDK::SetLogMessageCallback(&OfficialPipeClient::OnOfficialLog);

    if (!PipeSDK::CreatePipeClient(&client_)) {
      logger.Error("Official PipeSDK CreatePipeClient failed; pipeName=" + args.pipeName +
                   " maxChannels=" + std::to_string(args.maxChannels));
      client_ = nullptr;
      return false;
    }
    if (!client_) {
      logger.Error("Official PipeSDK CreatePipeClient returned success but client is null");
      return false;
    }

    client_->SetCallback(&OfficialPipeClient::OnOfficialEvent, this);
    const std::wstring pipeName(args.pipeName.begin(), args.pipeName.end());
    if (!client_->Open(pipeName.c_str(), static_cast<UINT32>(args.maxChannels))) {
      logger.Error("Official PipeSDK Open failed; pipeName=" + args.pipeName +
                   " maxChannels=" + std::to_string(args.maxChannels));
      client_->Release();
      client_ = nullptr;
      return false;
    }
    logger.Info("Official PipeSDK initialized; pipeName=" + args.pipeName +
                " maxChannels=" + std::to_string(args.maxChannels));
    return true;
#else
    logger.Error("Official PipeSDK is not vendored or not enabled. Add SDK files and configure with -DPLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK=ON.");
    return false;
#endif
  }

  bool Subscribe(const std::string& capability, Logger& logger) override {
#ifdef PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK
    if (!client_) {
      logger.Warn("Official PipeSDK not initialized; cannot subscribe: " + capability);
      return false;
    }

    if (capability == "comment" || capability == "like" || capability == "gift" ||
        capability == "fans_club" || capability == "follow") {
      if (openLiveDataSubscribed_) {
        logger.Info("OPEN_LIVE_DATA already subscribed; capability covered: " + capability);
        return true;
      }
      {
        std::lock_guard<std::mutex> lock(subscribeMutex_);
        pendingCapabilities_.insert(capability);
      }
      logger.Info("Queued official OPEN_LIVE_DATA subscription for capability: " + capability);
      if (connected_.load()) {
        StartSubscribeWorker();
      } else {
        logger.Info("Official pipe is not EVENT_CONNECTED yet; delaying OPEN_LIVE_DATA subscription");
      }
      return true;
    }

    if (capability == "total_like") {
      logger.Info("total_like kept as a query capability; no streaming subscription is registered in this minimal pass");
      return true;
    }

    if (capability == "enter") {
      logger.Warn("enter capability requires separate approval; official streaming subscription is not enabled in this pass");
      return true;
    }

    logger.Warn("Unknown official capability requested: " + capability);
    return false;
#else
    logger.Warn("Official PipeSDK not initialized; cannot subscribe: " + capability);
    return false;
#endif
  }

  bool SendRequest(const std::string& requestJson, Logger& logger) override {
#ifdef PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK
    if (!client_) {
      logger.Warn("Official PipeSDK not initialized; cannot send request: " + requestJson);
      return false;
    }
    const UINT32 msgId = ++messageId_;
    const bool ok = client_->SendMessage(msgId, requestJson.c_str(), static_cast<UINT32>(requestJson.size()));
    if (!ok) {
      logger.Warn("Official PipeSDK SendMessage failed; msgId=" + std::to_string(msgId) + " body=" + requestJson);
    } else {
      logger.Info("Official PipeSDK SendMessage ok; msgId=" + std::to_string(msgId) + " body=" + requestJson);
    }
    return ok;
#else
    logger.Warn("Official PipeSDK not initialized; cannot send request: " + requestJson);
    return false;
#endif
  }

  void SetEventCallback(EventCallback callback) override {
    callback_ = std::move(callback);
  }

  bool ShouldExit() const override {
    return disconnected_.load();
  }

  void Shutdown(Logger& logger) override {
#ifdef PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK
    disconnected_.store(true);
    JoinSubscribeWorker();
    if (client_) {
      client_->Close();
      client_->Release();
      client_ = nullptr;
    }
    if (activeLogger_ == &logger) {
      activeLogger_ = nullptr;
    }
#endif
    logger.Info("Official PipeClient shutdown");
  }

 private:
#ifdef PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK
  void StartSubscribeWorker() {
    if (subscribeWorkerStarted_.exchange(true)) {
      return;
    }
    subscribeWorker_ = std::thread([this]() { SubscribeAfterConnected(); });
  }

  void JoinSubscribeWorker() {
    if (subscribeWorker_.joinable()) {
      subscribeWorker_.join();
    }
  }

  void SubscribeAfterConnected() {
    if (logger_) logger_->Info("OPEN_LIVE_DATA subscribe worker waiting 500ms after EVENT_CONNECTED");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    for (int attempt = 1; attempt <= 3 && !disconnected_.load(); ++attempt) {
      if (!client_ || !connected_.load()) {
        if (logger_) logger_->Warn("OPEN_LIVE_DATA subscribe attempt " + std::to_string(attempt) +
                                   " skipped because official pipe is not connected");
      } else {
        const std::string reqId = "subscribe-open-live-data-" + GenerateUuid();
        const std::string params = "{\"eventName\":\"OPEN_LIVE_DATA\",\"timestamp\":" + std::to_string(NowMillis()) + "}";
        const std::string request = MakeRequestJson(reqId, "x.subscribeEvent", params);
        {
          std::lock_guard<std::mutex> lock(requestMutex_);
          pendingRequests_[reqId] = request;
        }
        if (logger_) logger_->Info("OPEN_LIVE_DATA subscribe attempt " + std::to_string(attempt) +
                                   "/3 request: " + request);
        if (SendRequest(request, *logger_)) {
          if (logger_) logger_->Info("OPEN_LIVE_DATA subscribe request sent; waiting for response reqId=" + reqId);
        }
        for (int waitIndex = 0; waitIndex < 15 && !disconnected_.load(); ++waitIndex) {
          if (openLiveDataSubscribed_) {
            if (logger_) logger_->Info("OPEN_LIVE_DATA subscribe response confirmed success");
            return;
          }
          std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    subscribeWorkerStarted_.store(false);
    if (logger_) logger_->Warn("OPEN_LIVE_DATA subscribe worker finished without a successful SendMessage");
  }

  void PersistRawMessage(UINT32 msg, const std::string& message) {
    const uint32_t index = ++rawMessageCount_;
    try {
      std::filesystem::create_directories("logs");
      std::ostringstream filename;
      filename << "logs/raw-event-" << std::setw(4) << std::setfill('0') << index << ".json";
      std::ofstream out(filename.str(), std::ios::binary);
      out << message;
      if (logger_) {
        logger_->Info("Official raw EVENT_MESSAGE persisted: " + filename.str() +
                      " msg=" + std::to_string(msg) + " bytes=" + std::to_string(message.size()));
      }
    } catch (const std::exception& ex) {
      if (logger_) logger_->Warn(std::string("Failed to persist official raw EVENT_MESSAGE: ") + ex.what());
    }
  }

  void LogRequestResponse(const std::string& response) {
    if (response.find("\"type\":\"request\"") == std::string::npos &&
        response.find("\"type\": \"request\"") == std::string::npos) {
      return;
    }

    const std::string reqId = ExtractJsonString(response, "reqId");
    const std::string code = ExtractJsonNumberToken(response, "code");
    std::string request;
    if (!reqId.empty()) {
      std::lock_guard<std::mutex> lock(requestMutex_);
      const auto found = pendingRequests_.find(reqId);
      if (found != pendingRequests_.end()) {
        request = found->second;
        pendingRequests_.erase(found);
      }
    }

    const bool isSubscribeResponse = request.find("\"method\":\"x.subscribeEvent\"") != std::string::npos ||
                                     reqId.find("subscribe-open-live-data-") == 0;
    if (isSubscribeResponse && code == "1") {
      openLiveDataSubscribed_ = true;
    }

    if (logger_) {
      logger_->Info("Official request response received; reqId=" + (reqId.empty() ? "<missing>" : reqId) +
                    " code=" + (code.empty() ? "<missing>" : code) +
                    " response=" + response);
      if (!request.empty()) {
        logger_->Info("Official matched request for reqId=" + reqId + ": " + request);
      }
      if (!code.empty() && code != "1") {
        logger_->Warn("Official request returned non-success code; reqId=" + (reqId.empty() ? "<missing>" : reqId) +
                      " request=" + (request.empty() ? "<unknown>" : request) +
                      " response=" + response);
      }
    }
  }

  static BOOL OnOfficialLog(PipeSDK::LogSeverity level, LPCSTR file, INT32 line, LPCSTR text) {
    if (!activeLogger_) return TRUE;
    std::ostringstream out;
    out << "PipeSDK[" << static_cast<int>(level) << "] " << (file ? file : "") << ":" << line << " " << (text ? text : "");
    activeLogger_->Info(out.str());
    return TRUE;
  }

  static void OnOfficialEvent(PipeSDK::IPC_EVENT_TYPE type, UINT32 msg, LPCSTR data, UINT32 size, void* args) {
    auto* self = static_cast<OfficialPipeClient*>(args);
    if (self) self->HandleOfficialEvent(type, msg, data, size);
  }

  void HandleOfficialEvent(PipeSDK::IPC_EVENT_TYPE type, UINT32 msg, LPCSTR data, UINT32 size) {
    if (type == PipeSDK::EVENT_CONNECTED) {
      connected_.store(true);
      if (logger_) logger_->Info("Official PipeSDK EVENT_CONNECTED received");
      StartSubscribeWorker();
      return;
    }

    if (type == PipeSDK::EVENT_DISCONNECTED) {
      connected_.store(false);
      disconnected_.store(true);
      if (logger_) logger_->Warn("Official PipeSDK EVENT_DISCONNECTED received; helper will exit gracefully");
      return;
    }

    if (type != PipeSDK::EVENT_MESSAGE) {
      if (logger_) logger_->Info("Official PipeSDK event ignored; type=" + std::to_string(static_cast<int>(type)) +
                                 " msg=" + std::to_string(msg));
      return;
    }

    const std::string message(data ? data : "", data && size > 0 ? size : 0);
    if (logger_) {
      logger_->Info("Official PipeSDK EVENT_MESSAGE received; msg=" + std::to_string(msg) +
                    " size=" + std::to_string(size));
      logger_->Info("Official raw EVENT_MESSAGE body (prefix 2000): " +
                    message.substr(0, 2000));
    }
    PersistRawMessage(msg, message);
    LogRequestResponse(message);

    const auto events = ParseOfficialPipeMessage(message);
    if (events.empty()) {
      if (message.find("\"code\":") != std::string::npos && logger_) {
        logger_->Info("Official PipeSDK request response or non-live-data message: " + message);
      }
      if (HasNonSuccessResponseCode(message) && callback_) {
        OfficialInteractionEvent systemEvent;
        systemEvent.type = OfficialEventType::System;
        systemEvent.sourceEventId = "official-response-" + GenerateUuid();
        systemEvent.timestampMs = NowMillis();
        systemEvent.text = "official request failed or capability is not available";
        systemEvent.rawJson = message;
        callback_(systemEvent);
      }
      return;
    }

    for (const OfficialInteractionEvent& event : events) {
      if (logger_) {
        logger_->Info("Official parsed live event type=" + std::to_string(static_cast<int>(event.type)) +
                      " eventId=" + event.sourceEventId +
                      " userId=" + event.userId +
                      " nickname=" + event.nickname);
      }
      if (callback_) callback_(event);
    }
  }

  PipeSDK::IPipeClient* client_ = nullptr;
  inline static Logger* activeLogger_ = nullptr;
  std::thread subscribeWorker_;
  std::mutex subscribeMutex_;
  std::set<std::string> pendingCapabilities_;
  std::mutex requestMutex_;
  std::map<std::string, std::string> pendingRequests_;
#endif

  EventCallback callback_;
  Logger* logger_ = nullptr;
  std::atomic<bool> disconnected_{false};
  std::atomic<bool> connected_{false};
  std::atomic<bool> subscribeWorkerStarted_{false};
  std::atomic<uint32_t> rawMessageCount_{0};
  uint32_t messageId_ = 0;
  bool openLiveDataSubscribed_ = false;
};

std::unique_ptr<IPipeClient> CreatePipeClient(bool mockMode) {
  if (mockMode) {
    return std::make_unique<MockPipeClient>();
  }
  return std::make_unique<OfficialPipeClient>();
}

}  // namespace plugin_bridge
