#pragma once

#include <functional>
#include <memory>
#include <string>

#include "args/launch_args.h"
#include "events/event_types.h"

namespace plugin_bridge {

class Logger;

using EventCallback = std::function<void(const OfficialInteractionEvent&)>;

class IPipeClient {
 public:
  virtual ~IPipeClient() = default;
  virtual bool Initialize(const LaunchArgs& args, Logger& logger) = 0;
  virtual bool Subscribe(const std::string& capability, Logger& logger) = 0;
  virtual bool SendRequest(const std::string& requestJson, Logger& logger) = 0;
  virtual void SetEventCallback(EventCallback callback) = 0;
  virtual void Shutdown(Logger& logger) = 0;
};

std::unique_ptr<IPipeClient> CreatePipeClient(bool mockMode);

}  // namespace plugin_bridge
