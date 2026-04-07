#include "events/event_parser.h"

#include <regex>

namespace plugin_bridge {

static std::string ExtractString(const std::string& json, const std::string& key) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]*)\"");
  std::smatch match;
  return std::regex_search(json, match, pattern) ? match[1].str() : "";
}

static int ExtractInt(const std::string& json, const std::string& key) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*([0-9]+)");
  std::smatch match;
  return std::regex_search(json, match, pattern) ? std::stoi(match[1].str()) : 0;
}

static int64_t ExtractInt64(const std::string& json, const std::string& key) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*([0-9]+)");
  std::smatch match;
  return std::regex_search(json, match, pattern) ? std::stoll(match[1].str()) : 0;
}

static OfficialEventType ParseType(const std::string& value) {
  if (value == "comment") return OfficialEventType::Comment;
  if (value == "like") return OfficialEventType::Like;
  if (value == "total_like") return OfficialEventType::TotalLike;
  if (value == "gift") return OfficialEventType::Gift;
  if (value == "fans_club") return OfficialEventType::FansClub;
  if (value == "follow") return OfficialEventType::Follow;
  if (value == "enter") return OfficialEventType::Enter;
  return OfficialEventType::Unknown;
}

std::optional<OfficialInteractionEvent> ParseCanonicalEventJson(const std::string& json) {
  OfficialInteractionEvent event;
  event.type = ParseType(ExtractString(json, "eventType"));
  if (event.type == OfficialEventType::Unknown) {
    return std::nullopt;
  }

  event.sourceEventId = ExtractString(json, "eventId");
  event.timestampMs = ExtractInt64(json, "timestamp");
  event.userId = ExtractString(json, "userId");
  event.nickname = ExtractString(json, "nickname");
  event.avatar = ExtractString(json, "avatar");
  event.fansLevel = ExtractInt(json, "fansLevel");
  event.text = ExtractString(json, "text");
  event.giftName = ExtractString(json, "giftName");
  event.giftCount = ExtractInt(json, "giftCount");
  event.likeCount = ExtractInt(json, "likeCount");
  event.totalLikeCount = ExtractInt64(json, "totalLikeCount");
  event.fansClubLevel = ExtractInt(json, "fansClubLevel");
  event.followAction = ExtractString(json, "followAction");
  event.rawJson = json;
  return event;
}

}  // namespace plugin_bridge
