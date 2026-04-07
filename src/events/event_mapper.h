#pragma once

#include <optional>

#include "config/app_config.h"
#include "events/event_types.h"
#include "model/live_event.h"

namespace plugin_bridge {

std::optional<LiveEvent> MapOfficialEvent(const OfficialInteractionEvent& input, const CapabilityConfig& capabilities);

}  // namespace plugin_bridge
