#pragma once

#include <string>

namespace plugin_bridge {

struct BridgeConfig {
  std::string url = "ws://127.0.0.1:17891";
  int reconnectMinMs = 500;
  int reconnectMaxMs = 10000;
};

struct LoggingConfig {
  std::string path = "logs/plugin-bridge-helper.log";
};

struct CapabilityConfig {
  bool comment = true;
  bool like = true;
  bool gift = true;
  bool fansClub = true;
  bool follow = true;
  bool enter = false;
  bool totalLikeCount = true;
};

struct SecurityConfig {
  std::string appSecretEnv = "DOUYIN_PLUGIN_APP_SECRET";
};

struct DebugConfig {
  bool mockMode = false;
  std::string mockEventFile;
};

struct AppConfig {
  BridgeConfig bridge;
  LoggingConfig logging;
  CapabilityConfig capabilities;
  SecurityConfig security;
  DebugConfig debug;
};

AppConfig LoadConfig(const std::string& path);
std::string ReadEnvSecret(const SecurityConfig& security);

}  // namespace plugin_bridge
