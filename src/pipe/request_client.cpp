#include "pipe/request_client.h"

#include "model/live_event.h"

namespace plugin_bridge {

std::string MakeSubscribeRequest(const std::string& capability) {
  return "{\"action\":\"subscribe\",\"capability\":\"" + JsonEscape(capability) + "\"}";
}

}  // namespace plugin_bridge
