#include "events/event_mapper.h"

#include "util/time_util.h"
#include "util/uuid.h"

namespace plugin_bridge {

std::optional<LiveEvent> MapOfficialEvent(const OfficialInteractionEvent& input, const CapabilityConfig& capabilities) {
  LiveEvent event;
  event.eventId = input.sourceEventId.empty() ? GenerateUuid() : input.sourceEventId;
  event.timestamp = input.timestampMs > 0 ? input.timestampMs : NowMillis();
  event.user.id = input.userId.empty() ? "unknown" : input.userId;
  event.user.nickname = input.nickname.empty() ? "unknown" : input.nickname;
  event.user.avatar = input.avatar;
  event.user.fansLevel = input.fansLevel;
  event.rawJson = input.rawJson.empty() ? "{}" : input.rawJson;

  switch (input.type) {
    case OfficialEventType::Comment:
      if (!capabilities.comment) return std::nullopt;
      event.type = "comment";
      event.payload.text = input.text;
      break;
    case OfficialEventType::Like:
      if (!capabilities.like) return std::nullopt;
      event.type = "like";
      event.payload.likeCount = input.likeCount > 0 ? input.likeCount : 1;
      break;
    case OfficialEventType::TotalLike:
      if (!capabilities.totalLikeCount) return std::nullopt;
      event.type = "like";
      event.payload.totalLikeCount = input.totalLikeCount;
      event.payload.text = "total_like_count";
      break;
    case OfficialEventType::Gift:
      if (!capabilities.gift) return std::nullopt;
      event.type = "gift";
      event.payload.giftName = input.giftName;
      event.payload.giftCount = input.giftCount > 0 ? input.giftCount : 1;
      break;
    case OfficialEventType::FansClub:
      if (!capabilities.fansClub) return std::nullopt;
      event.type = "fans_club";
      event.payload.fansClubLevel = input.fansClubLevel;
      event.payload.text = input.text;
      break;
    case OfficialEventType::Follow:
      if (!capabilities.follow) return std::nullopt;
      event.type = "follow";
      event.payload.followAction = input.followAction.empty() ? "follow" : input.followAction;
      event.payload.text = event.payload.followAction;
      break;
    case OfficialEventType::Enter:
      if (!capabilities.enter) return std::nullopt;
      event.type = "enter";
      event.payload.text = input.text.empty() ? "enter" : input.text;
      break;
    case OfficialEventType::Unknown:
      return std::nullopt;
  }

  return event;
}

}  // namespace plugin_bridge
