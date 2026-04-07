#pragma once

#include <string>
#include <vector>

#include "events/event_types.h"

namespace plugin_bridge {

std::vector<OfficialInteractionEvent> ParseOfficialPipeMessage(const std::string& json);

}  // namespace plugin_bridge
