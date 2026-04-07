#include "pipe/pipe_client.h"

#include "events/official_message_parser.h"
#include "logging/logger.h"
#include "util/time_util.h"
#include "util/uuid.h"

#include <atomic>
#include <cctype>
#include <cstdint>
#include <sstream>
#include <utility>

#ifdef PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK
#if __has_include(<PipeSDK.h>)
#include <PipeSDK.h>
#elif __has_include(<pipesdk.h>)
#include <pipesdk.h>
#elif __has_include(<PipeSDK/PipeSDK.h>)
#include <PipeSDK/PipeSDK.h>
#else
#error "PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK is ON, but no official PipeSDK header was found. Put the official SDK headers under third_party/PipeSDK/include."
#endif
#endif

namespace plugin_bridge {

namespace {

std::string MakeRequestJson(const std::string& reqId, const std::string& method, const std::string& paramsJson) {
  return "{\"type\":\"request\",\"reqId\":\"" + reqId + "\",\"method\":\"" + method + "\",\"params\":" + paramsJson + "}";
}

bool HasNonSuccessResponseCode(const std::string& json) {
  const size_t keyPos = json.find("\"code\"");
  if (keyPos == std::string::npos) return false;
  const size_t colon = json.find(':', keyPos + 6);
  if (colon == std::string::npos) return false;
  size_t value = colon + 1;
  while (value < json.size() && std::isspace(static_cast<unsigned char>(json[value]))) {
    ++value;
  }
  const size_t numberStart = value;
  if (value < json.size() && json[value] == '-') ++value;
  while (value < json.size() && std::isdigit(static_cast<unsigned char>(json[value]))) {
    ++value;
  }
  return json.substr(numberStart, value - numberStart) != "1";
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
  bool Initialize(const LaunchArgs& args, Logger& logger) override {
#ifdef PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK
    logger_ = &logger;
    activeLogger_ = &logger;
    disconnected_.store(false);

    PipeSDK::SetLogMessageCallback(&OfficialPipeClient::OnOfficialLog);

    if (!PipeSDK::CreatePipeClient(args.pipeName.c_str(), static_cast<UINT32>(args.maxChannels), &client_)) {
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

      const std::string params = "{\"eventName\":\"OPEN_LIVE_DATA\",\"timestamp\":" + std::to_string(NowMillis()) + "}";
      const bool ok = SendRequest(MakeRequestJson(GenerateUuid(), "x.subscribeEvent", params), logger);
      if (ok) {
        openLiveDataSubscribed_ = true;
        logger.Info("Subscribed official OPEN_LIVE_DATA for capability: " + capability);
      }
      return ok;
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
    client_ = nullptr;
    if (activeLogger_ == &logger) {
      activeLogger_ = nullptr;
    }
#endif
    logger.Info("Official PipeClient shutdown");
  }

 private:
#ifdef PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK
  static BOOL OnOfficialLog(PipeSDK::LogSeverity level, LPCSTR file, INT32 line, LPCSTR text) {
    if (!activeLogger_) return TRUE;
    std::ostringstream out;
    out << "PipeSDK[" << static_cast<int>(level) << "] " << (file ? file : "") << ":" << line << " " << (text ? text : "");
    activeLogger_->Info(out.str());
    return TRUE;
  }

  static BOOL OnOfficialEvent(PipeSDK::IPC_EVENT_TYPE type, UINT32 msg, LPCSTR data, UINT32 size, void* args) {
    auto* self = static_cast<OfficialPipeClient*>(args);
    if (!self) return TRUE;
    return self->HandleOfficialEvent(type, msg, data, size);
  }

  BOOL HandleOfficialEvent(PipeSDK::IPC_EVENT_TYPE type, UINT32 msg, LPCSTR data, UINT32 size) {
    if (type == PipeSDK::EVENT_DISCONNECTED) {
      disconnected_.store(true);
      if (logger_) logger_->Warn("Official PipeSDK EVENT_DISCONNECTED received; helper will exit gracefully");
      return TRUE;
    }

    if (type != PipeSDK::EVENT_MESSAGE) {
      if (logger_) logger_->Info("Official PipeSDK event ignored; type=" + std::to_string(static_cast<int>(type)) +
                                 " msg=" + std::to_string(msg));
      return TRUE;
    }

    const std::string message(data ? data : "", data && size > 0 ? size : 0);
    if (logger_) logger_->Info("Official PipeSDK EVENT_MESSAGE received; msg=" + std::to_string(msg) +
                               " size=" + std::to_string(size));
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
      return TRUE;
    }

    for (const OfficialInteractionEvent& event : events) {
      if (callback_) callback_(event);
    }
    return TRUE;
  }

  PipeSDK::IPipeClient* client_ = nullptr;
  inline static Logger* activeLogger_ = nullptr;
#endif

  EventCallback callback_;
  Logger* logger_ = nullptr;
  std::atomic<bool> disconnected_{false};
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
