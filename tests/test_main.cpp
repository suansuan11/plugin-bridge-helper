#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "args/launch_args.h"
#include "bridge/envelope.h"
#include "config/app_config.h"
#include "events/event_mapper.h"
#include "events/event_types.h"
#include "events/official_message_parser.h"
#include "model/live_event.h"

using namespace plugin_bridge;

static void Expect(bool condition, const std::string& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << std::endl;
    std::exit(1);
  }
}

static void TestLaunchArgs() {
  const char* argv[] = {
      "plugin-bridge-helper.exe",
      "--pipeName=abc",
      "--maxChannels=4",
      "--mateVersion=7.1.0",
      "--layoutMode=2",
      "--once"};
  const auto args = ParseLaunchArgs(6, const_cast<char**>(argv));
  Expect(args.pipeName == "abc", "pipeName parsed");
  Expect(args.maxChannels == 4, "maxChannels parsed");
  Expect(args.mateVersion == "7.1.0", "mateVersion parsed");
  Expect(args.layoutMode == "2", "layoutMode parsed");
  Expect(args.runOnce, "runOnce parsed");
  Expect(args.IsValidForCompanion(), "args valid");

  const char* rawArgv[] = {
      "plugin-bridge-helper.exe",
      "pipeName=rawPipe",
      "maxChannels=2",
      "mateVersion=8.0.0",
      "layoutMode=1"};
  const auto rawArgs = ParseLaunchArgs(5, const_cast<char**>(rawArgv));
  Expect(rawArgs.pipeName == "rawPipe", "raw pipeName parsed");
  Expect(rawArgs.IsValidForCompanion(), "raw args valid");
}

static void TestCommentMapping() {
  CapabilityConfig caps;
  OfficialInteractionEvent input;
  input.type = OfficialEventType::Comment;
  input.sourceEventId = "comment-1";
  input.timestampMs = 1710000000000;
  input.userId = "u1";
  input.nickname = "viewer";
  input.text = "hello";
  input.rawJson = "{\"official\":true}";

  const auto event = MapOfficialEvent(input, caps);
  Expect(event.has_value(), "comment mapped");
  Expect(event->type == "comment", "comment type");
  Expect(event->payload.text == "hello", "comment text");
  Expect(ToJson(*event).find("\"type\":\"comment\"") != std::string::npos, "comment json");
}

static void TestLikeGiftMapping() {
  CapabilityConfig caps;

  OfficialInteractionEvent like;
  like.type = OfficialEventType::Like;
  like.userId = "u2";
  like.nickname = "like-user";
  like.likeCount = 3;
  const auto likeEvent = MapOfficialEvent(like, caps);
  Expect(likeEvent.has_value(), "like mapped");
  Expect(likeEvent->payload.likeCount == 3, "like count");

  OfficialInteractionEvent gift;
  gift.type = OfficialEventType::Gift;
  gift.userId = "u3";
  gift.nickname = "gift-user";
  gift.giftName = "Small Heart";
  gift.giftCount = 2;
  const auto giftEvent = MapOfficialEvent(gift, caps);
  Expect(giftEvent.has_value(), "gift mapped");
  Expect(giftEvent->type == "gift", "gift type");
  Expect(giftEvent->payload.giftName == "Small Heart", "gift name");
}

static void TestOpenLiveDataParsing() {
  const std::string message =
      R"({"type":"event","eventName":"OPEN_LIVE_DATA","params":{"payload":[)"
      R"({"msg_id":"like-1","timestamp":1711939193044,"msg_type":1,"msg_type_str":"live_like","sec_open_id":"open-like","avatar_url":"https://example.test/a.png","nickname":"Alice","like_num":2,"user_privilege_level":7,"fansclub_level":3},)"
      R"({"msg_id":"comment-1","timestamp":1711939362000,"msg_type":2,"msg_type_str":"live_comment","sec_open_id":"open-comment","avatar_url":"https://example.test/b.png","nickname":"Bob","content":"hello from official","user_privilege_level":5,"fansclub_level":2},)"
      R"({"msg_id":"gift-1","timestamp":1711939405000,"msg_type":3,"msg_type_str":"live_gift","sec_open_id":"open-gift","avatar_url":"https://example.test/c.png","nickname":"Carol","gift_name":"Rose","gift_num":4,"sec_gift_id":"gift-sec","user_privilege_level":1,"fansclub_level":0})"
      R"(]}})";

  const auto events = ParseOfficialPipeMessage(message);
  Expect(events.size() == 3, "OPEN_LIVE_DATA parsed payload size");
  Expect(events[0].type == OfficialEventType::Like, "official like type");
  Expect(events[0].sourceEventId == "like-1", "official like msg id");
  Expect(events[0].userId == "open-like", "official like sec_open_id");
  Expect(events[0].likeCount == 2, "official like count");
  Expect(events[1].type == OfficialEventType::Comment, "official comment type");
  Expect(events[1].text == "hello from official", "official comment content");
  Expect(events[1].fansLevel == 5, "official comment privilege level");
  Expect(events[1].fansClubLevel == 2, "official comment fansclub level");
  Expect(events[2].type == OfficialEventType::Gift, "official gift type");
  Expect(events[2].giftName == "Rose", "official gift name");
  Expect(events[2].giftCount == 4, "official gift count");

  const std::string documentedGiftWithoutName =
      R"({"type":"event","eventName":"OPEN_LIVE_DATA","params":{"payload":[)"
      R"({"msg_id":"gift-2","timestamp":1711939405000,"msg_type":3,"msg_type_str":"live_gift","sec_open_id":"open-gift","avatar_url":"https://example.test/c.png","nickname":"Carol","gift_num":1,"sec_gift_id":"gift-sec"})"
      R"(]}})";
  const auto giftOnlyEvents = ParseOfficialPipeMessage(documentedGiftWithoutName);
  Expect(giftOnlyEvents.size() == 1, "official gift without name is not dropped");
  Expect(giftOnlyEvents[0].giftName.empty(), "official missing gift_name stays empty");
  Expect(giftOnlyEvents[0].giftCount == 1, "official gift without name count");
}

static void TestEnterCapabilityGate() {
  CapabilityConfig caps;
  caps.enter = false;

  OfficialInteractionEvent enter;
  enter.type = OfficialEventType::Enter;
  enter.userId = "u4";
  enter.nickname = "enter-user";
  Expect(!MapOfficialEvent(enter, caps).has_value(), "enter gated off");

  caps.enter = true;
  Expect(MapOfficialEvent(enter, caps).has_value(), "enter gated on");
}

static void TestBridgeEnvelope() {
  LiveEvent event;
  event.eventId = "system-1";
  event.type = "system";
  event.timestamp = 1710000000000;
  event.user.id = "plugin";
  event.user.nickname = "Plugin";
  event.payload.text = "started";

  const std::string envelope = MakeBridgeEnvelope({event});
  Expect(envelope.find("douyin-live-overlay-bridge") != std::string::npos, "envelope protocol");
  Expect(envelope.find("\"events\"") != std::string::npos, "envelope events");
}

int main() {
  TestLaunchArgs();
  TestCommentMapping();
  TestLikeGiftMapping();
  TestOpenLiveDataParsing();
  TestEnterCapabilityGate();
  TestBridgeEnvelope();
  std::cout << "plugin_bridge_helper_tests passed" << std::endl;
  return 0;
}
