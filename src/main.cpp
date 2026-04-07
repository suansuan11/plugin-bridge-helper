#include <iostream>
#include <chrono>
#include <thread>

#include "args/launch_args.h"
#include "bridge/envelope.h"
#include "bridge/ws_publisher.h"
#include "config/app_config.h"
#include "events/event_mapper.h"
#include "events/event_types.h"
#include "logging/logger.h"
#include "model/live_event.h"
#include "pipe/pipe_client.h"
#include "pipe/request_client.h"
#include "util/time_util.h"
#include "util/uuid.h"

using namespace plugin_bridge;

static LiveEvent MakeSystemEvent(const std::string& text) {
  LiveEvent event;
  event.eventId = "plugin-helper-system-" + GenerateUuid();
  event.type = "system";
  event.timestamp = NowMillis();
  event.user.id = "plugin-bridge-helper";
  event.user.nickname = "Plugin Bridge Helper";
  event.payload.text = text;
  event.rawJson = "{}";
  return event;
}

int main(int argc, char** argv) {
  const LaunchArgs launchArgs = ParseLaunchArgs(argc, argv);
  AppConfig config = LoadConfig(launchArgs.configPath);
  if (launchArgs.mockMode) {
    config.debug.mockMode = true;
  }

  Logger logger;
  logger.Open(config.logging.path);
  logger.Info("plugin-bridge-helper starting");
  logger.Info("pipeName=" + launchArgs.pipeName + " maxChannels=" + std::to_string(launchArgs.maxChannels) +
              " mateVersion=" + launchArgs.mateVersion + " layoutMode=" + launchArgs.layoutMode);

  if (!launchArgs.IsValidForCompanion() && !config.debug.mockMode) {
    logger.Error("Missing required Live Companion launch args. Use --mock for local debug.");
    return 2;
  }

  if (ReadEnvSecret(config.security).empty()) {
    logger.Warn("App secret env value is empty for key: " + config.security.appSecretEnv);
  }

  WsPublisher publisher(config.bridge, logger);
  publisher.Connect();
  publisher.Publish(MakeBridgeEnvelope({MakeSystemEvent("plugin-bridge-helper started")}));

  auto pipeClient = CreatePipeClient(config.debug.mockMode);
  pipeClient->SetEventCallback([&](const OfficialInteractionEvent& officialEvent) {
    const auto mapped = MapOfficialEvent(officialEvent, config.capabilities);
    if (!mapped.has_value()) {
      publisher.Publish(MakeBridgeEnvelope({MakeSystemEvent("event dropped by capability gate or unknown type")}));
      return;
    }
    publisher.Publish(MakeBridgeEnvelope({*mapped}));
  });

  if (!pipeClient->Initialize(launchArgs, logger)) {
    publisher.Publish(MakeBridgeEnvelope({MakeSystemEvent("PipeSDK initialization failed or SDK is not configured")}));
    if (!config.debug.mockMode) {
      return 3;
    }
  }

  const char* capabilities[] = {"comment", "like", "gift", "fans_club", "follow", "total_like"};
  for (const char* capability : capabilities) {
    pipeClient->Subscribe(capability, logger);
  }
  if (config.capabilities.enter) {
    pipeClient->Subscribe("enter", logger);
  } else {
    logger.Warn("enter capability is disabled; user-enter events will be ignored");
    publisher.Publish(MakeBridgeEnvelope({MakeSystemEvent("enter capability disabled or not approved")}));
  }

  if (launchArgs.runOnce) {
    pipeClient->Shutdown(logger);
    logger.Info("plugin-bridge-helper exiting after --once");
    return 0;
  }

  logger.Info("plugin-bridge-helper running. Press Ctrl+C or close process to exit.");
  while (true) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}
