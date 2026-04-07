#include "config/app_config.h"

#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>

namespace plugin_bridge {

static std::string ReadFile(const std::string& path) {
  std::ifstream file(path);
  if (!file) return "";
  std::ostringstream buffer;
  buffer << file.rdbuf();
  return buffer.str();
}

static std::string ExtractString(const std::string& json, const std::string& key, const std::string& fallback) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
  std::smatch match;
  return std::regex_search(json, match, pattern) ? match[1].str() : fallback;
}

static int ExtractInt(const std::string& json, const std::string& key, int fallback) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*([0-9]+)");
  std::smatch match;
  return std::regex_search(json, match, pattern) ? std::stoi(match[1].str()) : fallback;
}

static bool ExtractBool(const std::string& json, const std::string& key, bool fallback) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*(true|false)");
  std::smatch match;
  return std::regex_search(json, match, pattern) ? match[1].str() == "true" : fallback;
}

AppConfig LoadConfig(const std::string& path) {
  AppConfig config;
  const std::string json = ReadFile(path);
  if (json.empty()) {
    return config;
  }

  config.bridge.url = ExtractString(json, "url", config.bridge.url);
  config.bridge.reconnectMinMs = ExtractInt(json, "reconnectMinMs", config.bridge.reconnectMinMs);
  config.bridge.reconnectMaxMs = ExtractInt(json, "reconnectMaxMs", config.bridge.reconnectMaxMs);
  config.logging.path = ExtractString(json, "path", config.logging.path);
  config.capabilities.comment = ExtractBool(json, "comment", config.capabilities.comment);
  config.capabilities.like = ExtractBool(json, "like", config.capabilities.like);
  config.capabilities.gift = ExtractBool(json, "gift", config.capabilities.gift);
  config.capabilities.fansClub = ExtractBool(json, "fansClub", config.capabilities.fansClub);
  config.capabilities.follow = ExtractBool(json, "follow", config.capabilities.follow);
  config.capabilities.enter = ExtractBool(json, "enter", config.capabilities.enter);
  config.capabilities.totalLikeCount = ExtractBool(json, "totalLikeCount", config.capabilities.totalLikeCount);
  config.security.appSecretEnv = ExtractString(json, "appSecretEnv", config.security.appSecretEnv);
  config.debug.mockMode = ExtractBool(json, "mockMode", config.debug.mockMode);
  config.debug.mockEventFile = ExtractString(json, "mockEventFile", config.debug.mockEventFile);
  return config;
}

std::string ReadEnvSecret(const SecurityConfig& security) {
  const char* value = std::getenv(security.appSecretEnv.c_str());
  return value == nullptr ? "" : std::string(value);
}

}  // namespace plugin_bridge
