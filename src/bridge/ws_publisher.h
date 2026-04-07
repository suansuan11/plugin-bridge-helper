#pragma once

#include <mutex>
#include <string>

#include "config/app_config.h"

namespace plugin_bridge {

class Logger;

class WsPublisher {
 public:
  WsPublisher(BridgeConfig config, Logger& logger);
  ~WsPublisher();

  bool Connect();
  void Close();
  bool Publish(const std::string& json);

 private:
  BridgeConfig config_;
  Logger& logger_;
  std::mutex mutex_;

#ifdef _WIN32
  void* session_ = nullptr;
  void* connection_ = nullptr;
  void* request_ = nullptr;
  void* websocket_ = nullptr;
#endif
};

}  // namespace plugin_bridge
