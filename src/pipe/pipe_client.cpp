#include "pipe/pipe_client.h"

#include "logging/logger.h"
#include "util/time_util.h"
#include "util/uuid.h"

#include <utility>

namespace plugin_bridge {

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
    event.nickname = "Mock 用户";
    event.rawJson = "{\"mock\":true,\"capability\":\"" + capability + "\"}";

    if (capability == "comment") {
      event.type = OfficialEventType::Comment;
      event.text = "Mock 评论事件";
      callback_(event);
    } else if (capability == "like") {
      event.type = OfficialEventType::Like;
      event.likeCount = 1;
      callback_(event);
    } else if (capability == "gift") {
      event.type = OfficialEventType::Gift;
      event.giftName = "Mock 礼物";
      event.giftCount = 1;
      callback_(event);
    } else if (capability == "fans_club") {
      event.type = OfficialEventType::FansClub;
      event.fansClubLevel = 3;
      event.text = "Mock 粉丝团事件";
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
    logger.Info("Official PipeSDK build flag enabled; wire official demo API here. pipeName=" + args.pipeName);
    return false;
#else
    logger.Error("Official PipeSDK is not vendored. Build with PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK after adding SDK files.");
    return false;
#endif
  }

  bool Subscribe(const std::string& capability, Logger& logger) override {
    logger.Warn("Official PipeSDK not initialized; cannot subscribe: " + capability);
    return false;
  }

  bool SendRequest(const std::string& requestJson, Logger& logger) override {
    logger.Warn("Official PipeSDK not initialized; cannot send request: " + requestJson);
    return false;
  }

  void SetEventCallback(EventCallback callback) override {
    callback_ = std::move(callback);
  }

  void Shutdown(Logger& logger) override {
    logger.Info("Official PipeClient shutdown");
  }

 private:
  EventCallback callback_;
};

std::unique_ptr<IPipeClient> CreatePipeClient(bool mockMode) {
  if (mockMode) {
    return std::make_unique<MockPipeClient>();
  }
  return std::make_unique<OfficialPipeClient>();
}

}  // namespace plugin_bridge
