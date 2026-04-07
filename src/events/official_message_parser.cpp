#include "events/official_message_parser.h"

#include <cctype>
#include <cstdlib>
#include <string>
#include <vector>

namespace plugin_bridge {
namespace {

bool IsEscaped(const std::string& value, size_t quote) {
  size_t backslashes = 0;
  while (quote > backslashes && value[quote - backslashes - 1] == '\\') {
    ++backslashes;
  }
  return backslashes % 2 == 1;
}

std::string UnescapeJsonString(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  for (size_t index = 0; index < value.size(); ++index) {
    if (value[index] != '\\' || index + 1 >= value.size()) {
      out.push_back(value[index]);
      continue;
    }
    const char escaped = value[++index];
    switch (escaped) {
      case '"': out.push_back('"'); break;
      case '\\': out.push_back('\\'); break;
      case '/': out.push_back('/'); break;
      case 'b': out.push_back('\b'); break;
      case 'f': out.push_back('\f'); break;
      case 'n': out.push_back('\n'); break;
      case 'r': out.push_back('\r'); break;
      case 't': out.push_back('\t'); break;
      default:
        out.push_back('\\');
        out.push_back(escaped);
        break;
    }
  }
  return out;
}

std::string ExtractString(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const size_t keyPos = json.find(needle);
  if (keyPos == std::string::npos) return "";
  const size_t colon = json.find(':', keyPos + needle.size());
  if (colon == std::string::npos) return "";
  size_t start = colon + 1;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) ++start;
  if (start >= json.size() || json[start] != '"') return "";
  ++start;
  for (size_t end = start; end < json.size(); ++end) {
    if (json[end] == '"' && !IsEscaped(json, end)) {
      return UnescapeJsonString(json.substr(start, end - start));
    }
  }
  return "";
}

int64_t ExtractInt64(const std::string& json, const std::string& key) {
  const std::string needle = "\"" + key + "\"";
  const size_t keyPos = json.find(needle);
  if (keyPos == std::string::npos) return 0;
  const size_t colon = json.find(':', keyPos + needle.size());
  if (colon == std::string::npos) return 0;
  size_t start = colon + 1;
  while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) ++start;
  const size_t numberStart = start;
  if (start < json.size() && json[start] == '-') ++start;
  while (start < json.size() && std::isdigit(static_cast<unsigned char>(json[start]))) ++start;
  if (start == numberStart) return 0;
  return std::strtoll(json.substr(numberStart, start - numberStart).c_str(), nullptr, 10);
}

std::vector<std::string> ExtractPayloadObjects(const std::string& json) {
  std::vector<std::string> objects;
  const std::string needle = "\"payload\"";
  const size_t keyPos = json.find(needle);
  if (keyPos == std::string::npos) return objects;
  const size_t colon = json.find(':', keyPos + needle.size());
  if (colon == std::string::npos) return objects;
  const size_t arrayStart = json.find('[', colon + 1);
  if (arrayStart == std::string::npos) return objects;

  bool inString = false;
  int depth = 0;
  size_t objectStart = std::string::npos;
  for (size_t index = arrayStart + 1; index < json.size(); ++index) {
    const char ch = json[index];
    if (ch == '"' && !IsEscaped(json, index)) {
      inString = !inString;
      continue;
    }
    if (inString) continue;
    if (ch == '{') {
      if (depth == 0) objectStart = index;
      ++depth;
    } else if (ch == '}') {
      --depth;
      if (depth == 0 && objectStart != std::string::npos) {
        objects.push_back(json.substr(objectStart, index - objectStart + 1));
        objectStart = std::string::npos;
      }
    } else if (ch == ']' && depth == 0) {
      break;
    }
  }
  return objects;
}

OfficialEventType ParsePayloadType(const std::string& object) {
  const std::string msgType = ExtractString(object, "msg_type_str");
  if (msgType == "live_like") return OfficialEventType::Like;
  if (msgType == "live_comment") return OfficialEventType::Comment;
  if (msgType == "live_gift") return OfficialEventType::Gift;
  if (msgType == "live_fansclub") return OfficialEventType::FansClub;
  if (msgType == "live_follow") return OfficialEventType::Follow;

  const int64_t numericType = ExtractInt64(object, "msg_type");
  if (numericType == 1) return OfficialEventType::Like;
  if (numericType == 2) return OfficialEventType::Comment;
  if (numericType == 3) return OfficialEventType::Gift;
  if (numericType == 4) return OfficialEventType::FansClub;
  if (numericType == 5) return OfficialEventType::Follow;
  return OfficialEventType::Unknown;
}

OfficialInteractionEvent ParsePayloadObject(const std::string& object) {
  OfficialInteractionEvent event;
  event.type = ParsePayloadType(object);
  event.sourceEventId = ExtractString(object, "msg_id");
  event.timestampMs = ExtractInt64(object, "timestamp");
  event.userId = ExtractString(object, "sec_open_id");
  event.nickname = ExtractString(object, "nickname");
  event.avatar = ExtractString(object, "avatar_url");
  event.fansLevel = static_cast<int>(ExtractInt64(object, "user_privilege_level"));
  event.fansClubLevel = static_cast<int>(ExtractInt64(object, "fansclub_level"));
  event.rawJson = object;

  switch (event.type) {
    case OfficialEventType::Comment:
      event.text = ExtractString(object, "content");
      break;
    case OfficialEventType::Like:
      event.likeCount = static_cast<int>(ExtractInt64(object, "like_num"));
      break;
    case OfficialEventType::Gift:
      event.giftName = ExtractString(object, "gift_name");
      event.giftCount = static_cast<int>(ExtractInt64(object, "gift_num"));
      break;
    case OfficialEventType::Follow:
      event.followAction = ExtractInt64(object, "user_follow_action") == 2 ? "unfollow" : "follow";
      break;
    default:
      break;
  }

  return event;
}

}  // namespace

std::vector<OfficialInteractionEvent> ParseOfficialPipeMessage(const std::string& json) {
  std::vector<OfficialInteractionEvent> events;
  if (ExtractString(json, "type") != "event") return events;
  if (ExtractString(json, "eventName") != "OPEN_LIVE_DATA") return events;

  for (const std::string& object : ExtractPayloadObjects(json)) {
    OfficialInteractionEvent event = ParsePayloadObject(object);
    if (event.type != OfficialEventType::Unknown) {
      events.push_back(std::move(event));
    }
  }
  return events;
}

}  // namespace plugin_bridge
