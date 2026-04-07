#pragma once

#include <cstdint>
#include <map>
#include <string>

namespace plugin_bridge {

struct LiveEventUser {
  std::string id;
  std::string nickname;
  std::string avatar;
  int fansLevel = 0;
};

struct LiveEventPayload {
  std::string text;
  std::string giftName;
  int giftCount = 0;
  int likeCount = 0;
  int64_t totalLikeCount = 0;
  int fansClubLevel = 0;
  std::string followAction;
};

struct LiveEvent {
  std::string eventId;
  std::string type;
  int64_t timestamp = 0;
  LiveEventUser user;
  LiveEventPayload payload;
  std::string rawJson;
};

std::string JsonEscape(const std::string& value);
std::string ToJson(const LiveEvent& event);

}  // namespace plugin_bridge
