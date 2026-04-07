#pragma once

#include <string>
#include <vector>

#include "model/live_event.h"

namespace plugin_bridge {

std::string MakeBridgeEnvelope(const std::vector<LiveEvent>& events);

}  // namespace plugin_bridge
