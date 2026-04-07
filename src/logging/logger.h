#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace plugin_bridge {

class Logger {
 public:
  bool Open(const std::string& path);
  void Info(const std::string& message);
  void Warn(const std::string& message);
  void Error(const std::string& message);

 private:
  void Write(const std::string& level, const std::string& message);

  std::mutex mutex_;
  std::ofstream file_;
};

}  // namespace plugin_bridge
