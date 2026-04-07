#include "model/live_event.h"

#include <sstream>

namespace plugin_bridge {

std::string JsonEscape(const std::string& value) {
  std::ostringstream out;
  for (const char c : value) {
    switch (c) {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if (static_cast<unsigned char>(c) < 0x20) {
          out << "\\u00";
          const char* hex = "0123456789abcdef";
          out << hex[(c >> 4) & 0x0f] << hex[c & 0x0f];
        } else {
          out << c;
        }
    }
  }
  return out.str();
}

static void AppendStringField(std::ostringstream& out, const std::string& key, const std::string& value, bool& first) {
  if (value.empty()) return;
  if (!first) out << ",";
  first = false;
  out << "\"" << key << "\":\"" << JsonEscape(value) << "\"";
}

template <typename T>
static void AppendNumberField(std::ostringstream& out, const std::string& key, T value, bool& first) {
  if (value == 0) return;
  if (!first) out << ",";
  first = false;
  out << "\"" << key << "\":" << value;
}

std::string ToJson(const LiveEvent& event) {
  std::ostringstream out;
  out << "{";
  out << "\"eventId\":\"" << JsonEscape(event.eventId) << "\",";
  out << "\"type\":\"" << JsonEscape(event.type) << "\",";
  out << "\"timestamp\":" << event.timestamp << ",";
  out << "\"user\":{";
  bool firstUser = true;
  AppendStringField(out, "id", event.user.id, firstUser);
  AppendStringField(out, "nickname", event.user.nickname, firstUser);
  AppendStringField(out, "avatar", event.user.avatar, firstUser);
  AppendNumberField(out, "fansLevel", event.user.fansLevel, firstUser);
  out << "},\"payload\":{";
  bool firstPayload = true;
  AppendStringField(out, "text", event.payload.text, firstPayload);
  AppendStringField(out, "giftName", event.payload.giftName, firstPayload);
  AppendNumberField(out, "giftCount", event.payload.giftCount, firstPayload);
  AppendNumberField(out, "likeCount", event.payload.likeCount, firstPayload);
  AppendNumberField(out, "totalLikeCount", event.payload.totalLikeCount, firstPayload);
  AppendNumberField(out, "fansClubLevel", event.payload.fansClubLevel, firstPayload);
  AppendStringField(out, "followAction", event.payload.followAction, firstPayload);
  out << "}";
  if (!event.rawJson.empty()) {
    out << ",\"raw\":" << event.rawJson;
  }
  out << "}";
  return out.str();
}

}  // namespace plugin_bridge
