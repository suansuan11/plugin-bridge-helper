#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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
  void AcceptLoop();

  std::atomic_bool running_{false};
  bool wsaStarted_ = false;
  std::atomic<std::uintptr_t> listenSocket_{0};
  std::thread acceptThread_;
  std::vector<std::uintptr_t> clients_;
#endif
};

}  // namespace plugin_bridge
