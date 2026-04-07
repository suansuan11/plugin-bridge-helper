#pragma once

#include <string>

namespace plugin_bridge {

struct LaunchArgs {
  std::string pipeName;
  int maxChannels = 0;
  std::string mateVersion;
  std::string layoutMode;
  std::string configPath = "config/config.example.json";
  bool mockMode = false;
  bool runOnce = false;

  bool IsValidForCompanion() const;
};

LaunchArgs ParseLaunchArgs(int argc, char** argv);

}  // namespace plugin_bridge
