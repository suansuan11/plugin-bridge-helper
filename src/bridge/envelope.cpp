#include "bridge/envelope.h"

#include <sstream>

namespace plugin_bridge {

std::string MakeBridgeEnvelope(const std::vector<LiveEvent>& events) {
  std::ostringstream out;
  out << "{\"protocol\":\"douyin-live-overlay-bridge\",\"version\":1,\"events\":[";
  for (size_t index = 0; index < events.size(); ++index) {
    if (index > 0) out << ",";
    out << ToJson(events[index]);
  }
  out << "]}";
  return out.str();
}

}  // namespace plugin_bridge
