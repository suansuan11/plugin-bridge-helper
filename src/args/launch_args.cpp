#include "args/launch_args.h"

#include <cstdlib>
#include <string>

namespace plugin_bridge {

static bool StartsWith(const std::string& value, const std::string& prefix) {
  return value.rfind(prefix, 0) == 0;
}

static std::string ReadValue(const std::string& arg, const std::string& key, int& index, int argc, char** argv) {
  const std::string withEquals = "--" + key + "=";
  const std::string rawWithEquals = key + "=";
  const std::string slashWithEquals = "/" + key + "=";
  if (StartsWith(arg, withEquals)) {
    return arg.substr(withEquals.size());
  }
  if (StartsWith(arg, rawWithEquals)) {
    return arg.substr(rawWithEquals.size());
  }
  if (StartsWith(arg, slashWithEquals)) {
    return arg.substr(slashWithEquals.size());
  }
  if (arg == "--" + key && index + 1 < argc) {
    index += 1;
    return argv[index];
  }
  return "";
}

bool LaunchArgs::IsValidForCompanion() const {
  return !pipeName.empty() && maxChannels > 0 && !mateVersion.empty() && !layoutMode.empty();
}

LaunchArgs ParseLaunchArgs(int argc, char** argv) {
  LaunchArgs result;
  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    std::string value;
    if (!(value = ReadValue(arg, "pipeName", index, argc, argv)).empty()) result.pipeName = value;
    else if (!(value = ReadValue(arg, "maxChannels", index, argc, argv)).empty()) result.maxChannels = std::atoi(value.c_str());
    else if (!(value = ReadValue(arg, "mateVersion", index, argc, argv)).empty()) result.mateVersion = value;
    else if (!(value = ReadValue(arg, "layoutMode", index, argc, argv)).empty()) result.layoutMode = value;
    else if (!(value = ReadValue(arg, "config", index, argc, argv)).empty()) result.configPath = value;
    else if (arg == "--mock") result.mockMode = true;
    else if (arg == "--once") result.runOnce = true;
  }
  return result;
}

}  // namespace plugin_bridge
