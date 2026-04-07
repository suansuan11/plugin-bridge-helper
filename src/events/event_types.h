#pragma once

#include <cstdint>
#include <string>

namespace plugin_bridge {

enum class OfficialEventType {
  Comment,
  Like,
  TotalLike,
  Gift,
  FansClub,
  Follow,
  Enter,
  System,
  Unknown
};

struct OfficialInteractionEvent {
  OfficialEventType type = OfficialEventType::Unknown;
  std::string sourceEventId;
  int64_t timestampMs = 0;
  std::string userId;
  std::string nickname;
  std::string avatar;
  int fansLevel = 0;
  std::string text;
  std::string giftName;
  int giftCount = 0;
  int likeCount = 0;
  int64_t totalLikeCount = 0;
  int fansClubLevel = 0;
  std::string followAction;
  std::string rawJson;
};

}  // namespace plugin_bridge
