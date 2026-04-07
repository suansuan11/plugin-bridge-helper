#include "logging/logger.h"

#include <chrono>
#include <filesystem>
#include <iostream>

namespace plugin_bridge {

bool Logger::Open(const std::string& path) {
  std::lock_guard<std::mutex> lock(mutex_);
  const std::filesystem::path filePath(path);
  if (filePath.has_parent_path()) {
    std::filesystem::create_directories(filePath.parent_path());
  }
  file_.open(path, std::ios::app);
  return file_.is_open();
}

void Logger::Info(const std::string& message) { Write("INFO", message); }
void Logger::Warn(const std::string& message) { Write("WARN", message); }
void Logger::Error(const std::string& message) { Write("ERROR", message); }

void Logger::Write(const std::string& level, const std::string& message) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  const std::string line = "[" + level + "] " + std::to_string(static_cast<long long>(now)) + " " + message;
  std::cout << line << std::endl;
  if (file_.is_open()) {
    file_ << line << std::endl;
  }
}

}  // namespace plugin_bridge
