# plugin-bridge-helper

Windows C++17 helper for the Douyin Live Companion interactive plugin path.

The Electron overlay remains display-only. Set the overlay data source to `Bridge Receiver` and use:

```text
ws://127.0.0.1:17891
```

This helper connects to the official PipeSDK when Douyin Live Companion starts it with:

```text
--pipeName=<pipe> --maxChannels=<n> --mateVersion=<version> --layoutMode=<mode>
```

No packet capture, reverse engineering, private protocol, or hardcoded AppSecret belongs in this project.

## Current Repository Gap

This checkout currently contains only:

```text
third_party/PipeSDK/README.md
```

The real official SDK headers, import library, DLL, and demo source are not vendored here. The official branch in `src/pipe/pipe_client.cpp` is wired to the documented PipeSDK API, but a real official build still needs the SDK files below.

## Required Official Files

Download these from the official Douyin Open Platform "interactive tools, Live Companion only" C++ guide:

```text
pure_PipeSDK.zip
or PipeSDK_FFmpeg.zip
ConsoleApplication_source.zip
Live Companion debug package, x64 recommended for x64 builds
```

Recommended local layout:

```text
plugin-bridge-helper/third_party/PipeSDK/
  include/
    PipeSDK.h
  lib/
    PipeSDK.lib
  bin/
    PipeSDK.dll
  demo/
    ConsoleApplication_source/
```

The helper also auto-detects the current official package layout when present:

```text
plugin-bridge-helper/PipeSDK_FFmpeg/
  PipeDef.h
  lib/x64/PipeSDK.lib
  bin/x64/PipeSDK.dll
```

If the official package uses a different layout, keep the files as-is and pass:

```powershell
-DPLUGIN_BRIDGE_PIPESDK_ROOT="C:/path/to/PipeSDK"
```

## Build

Mock/local build:

```powershell
cmake -S plugin-bridge-helper -B plugin-bridge-helper/build -G "Visual Studio 17 2022" -A x64
cmake --build plugin-bridge-helper/build --config Release
ctest --test-dir plugin-bridge-helper/build -C Release --output-on-failure
```

Official PipeSDK build:

```powershell
cmake -S plugin-bridge-helper -B plugin-bridge-helper/build-official -G "Visual Studio 17 2022" -A x64 -DPLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK=ON
cmake --build plugin-bridge-helper/build-official --config Release
ctest --test-dir plugin-bridge-helper/build-official -C Release --output-on-failure
```

CMake checks for `PipeSDK.h`, `PipeSDK.lib`, and `PipeSDK.dll`. If the DLL is found, it is copied next to `plugin-bridge-helper.exe` after build. If the DLL is not copied, place it beside the exe manually before launching from Live Companion.

## Live Companion Debug Flow

1. Build `plugin-bridge-helper.exe` with the official SDK flag.
2. Start the existing Electron overlay.
3. Enter overlay edit mode.
4. Select `Bridge Receiver`.
5. Set the URL to `ws://127.0.0.1:17891`.
6. Open the official Live Companion debug package.
7. Open the interactive plugin debug panel.
8. Select the built `plugin-bridge-helper.exe` as the startup file.
9. Click the panel button to start the plugin and let Live Companion pass `pipeName`, `maxChannels`, `mateVersion`, and `layoutMode`.
10. Use the debug panel's simulated interaction messages, or another account in a live test room, to send comment, like, and gift events.
11. Confirm the overlay shows the mapped bridge events.

Do not run official mode manually without companion launch args. For local debugging without Live Companion, use mock mode:

```powershell
plugin-bridge-helper.exe --mock --once pipeName=localPipe maxChannels=1 mateVersion=debug layoutMode=0 --config config/config.example.json
```

## Official PipeSDK Path Implemented Here

The official path uses:

```text
PipeSDK::SetLogMessageCallback
PipeSDK::CreatePipeClient(pipeName, maxChannels, &client)
client->SetCallback(...)
client->SendMessage(msgId, json, size)
```

The helper subscribes once to:

```json
{
  "type": "request",
  "method": "x.subscribeEvent",
  "params": {
    "eventName": "OPEN_LIVE_DATA",
    "timestamp": 1710000000000
  }
}
```

The official callback parses `PipeSDK::EVENT_MESSAGE` JSON with:

```text
type="event"
eventName="OPEN_LIVE_DATA"
params.payload[]
```

Implemented payload mapping:

```text
live_comment -> comment
  msg_id -> eventId
  timestamp -> timestamp
  sec_open_id -> user.id
  nickname -> user.nickname
  avatar_url -> user.avatar
  user_privilege_level -> user.fansLevel
  fansclub_level -> payload.fansClubLevel/raw
  content -> payload.text

live_like -> like
  msg_id -> eventId
  timestamp -> timestamp
  sec_open_id -> user.id
  nickname -> user.nickname
  avatar_url -> user.avatar
  like_num -> payload.likeCount

live_gift -> gift
  msg_id -> eventId
  timestamp -> timestamp
  sec_open_id -> user.id
  nickname -> user.nickname
  avatar_url -> user.avatar
  gift_num -> payload.giftCount
  gift_name -> payload.giftName only if present in confirmed official data
  sec_gift_id remains in raw JSON when gift_name is absent
```

When `PipeSDK::EVENT_DISCONNECTED` is received, the helper logs it, shuts down gracefully, and exits as required by the official guide.

## Capabilities

Real bridge path implemented in this helper:

```text
comment -> overlay
like -> overlay
gift -> overlay, with giftCount and raw sec_gift_id; giftName only when official data provides a confirmed name
official SDK log callback
official SDK message callback
official disconnect exit
```

Kept but not expanded in this pass:

```text
fans_club
follow
total like count
enter/user-room events
```

`enter` remains disabled by default because user-room events may require separate platform approval.

## Troubleshooting

Helper starts but does not connect to pipe:

```text
Confirm Live Companion, not a manual shell, launched the exe.
Check the log for pipeName/maxChannels/mateVersion/layoutMode.
Confirm PipeSDK.dll is beside plugin-bridge-helper.exe.
```

Official build fails at CMake configure:

```text
Check third_party/PipeSDK/include for PipeSDK.h.
Check third_party/PipeSDK/lib for PipeSDK.lib.
Set PLUGIN_BRIDGE_PIPESDK_ROOT if the official package layout differs.
```

Connected to pipe but overlay shows nothing:

```text
Start the overlay first.
Set data source to Bridge Receiver.
Use ws://127.0.0.1:17891.
Check the helper log for OPEN_LIVE_DATA subscription and EVENT_MESSAGE lines.
```

Comment works but like/gift do not:

```text
Confirm the platform capability is approved.
Use the Live Companion debug panel simulated interaction messages first.
Check helper logs for non-success request responses.
Gift events may include sec_gift_id without gift_name; the raw JSON is preserved.
```

WebSocket send fails:

```text
The helper logs the failure and keeps running.
Restart or reselect Bridge Receiver in the overlay, then send another interaction event.
```

Capability is not open:

```text
The helper does not crash.
Subscription send failures and non-success official responses are logged and surfaced as system bridge events when possible.
```
