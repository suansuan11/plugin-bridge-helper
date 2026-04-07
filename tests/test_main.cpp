#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "args/launch_args.h"
#include "bridge/envelope.h"
#include "config/app_config.h"
#include "events/event_mapper.h"
#include "events/event_types.h"
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
    "--once"
  };
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
    "layoutMode=1"
  };
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
  input.nickname = "观众";
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
  like.nickname = "点赞用户";
  like.likeCount = 3;
  const auto likeEvent = MapOfficialEvent(like, caps);
  Expect(likeEvent.has_value(), "like mapped");
  Expect(likeEvent->payload.likeCount == 3, "like count");

  OfficialInteractionEvent gift;
  gift.type = OfficialEventType::Gift;
  gift.userId = "u3";
  gift.nickname = "送礼用户";
  gift.giftName = "小心心";
  gift.giftCount = 2;
  const auto giftEvent = MapOfficialEvent(gift, caps);
  Expect(giftEvent.has_value(), "gift mapped");
  Expect(giftEvent->type == "gift", "gift type");
  Expect(giftEvent->payload.giftName == "小心心", "gift name");
}

static void TestEnterCapabilityGate() {
  CapabilityConfig caps;
  caps.enter = false;
  OfficialInteractionEvent enter;
  enter.type = OfficialEventType::Enter;
  enter.userId = "u4";
  enter.nickname = "进房用户";
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
  TestEnterCapabilityGate();
  TestBridgeEnvelope();
  std::cout << "plugin_bridge_helper_tests passed" << std::endl;
  return 0;
}
