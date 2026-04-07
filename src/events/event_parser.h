#pragma once

#include <optional>
#include <string>

#include "events/event_types.h"

namespace plugin_bridge {

std::optional<OfficialInteractionEvent> ParseCanonicalEventJson(const std::string& json);

}  // namespace plugin_bridge
