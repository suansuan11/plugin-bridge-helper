# plugin-bridge-helper

Windows C++17 helper for Douyin Live Companion interactive plugins.

This project is intentionally separate from the Electron overlay. The overlay remains a display layer and should be set to `Bridge Receiver` with:

```text
ws://127.0.0.1:17891
```

## Why This Helper Exists

Douyin Live Companion starts an interactive plugin as an `.exe` and passes launch arguments such as `pipeName`, `maxChannels`, `mateVersion`, and `layoutMode`. This helper is the native Windows process that should connect to the official PipeSDK / interactive plugin SDK, subscribe to official live interaction events, map them to the overlay `LiveEvent` model, and publish them to the overlay through local WebSocket Bridge.

No packet capture, reverse engineering, or private protocol logic belongs here.

## Current Boundary

Implemented now:

```text
C++17 / CMake / Visual Studio-compatible project
launch argument parsing: pipeName / maxChannels / mateVersion / layoutMode
config loading with secret read from environment variable name
console + file logging
unified LiveEvent JSON serialization
official-event-like internal struct -> LiveEvent mapping
capability gating, including enter/user-room events disabled by default
Bridge envelope generation
WinHTTP WebSocket publisher on Windows
mock PipeClient for local debug and unit tests
official PipeSDK integration seam in pipe_client.cpp
unit tests for args, comment, like, gift, enter gating, envelope
```

Not implemented without official SDK files:

```text
Actual Douyin PipeSDK function calls
actual official callback registration
actual official subscription API payloads
actual user-enter capability verification
```

The official SDK is not vendored in this repository. Put the official SDK/demo files into:

```text
third_party/PipeSDK/
```

Then implement the `PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK` branch in:

```text
src/pipe/pipe_client.cpp
```

using only official headers/demo code.

## Directory

```text
plugin-bridge-helper/
  CMakeLists.txt
  config/config.example.json
  samples/sample-config.json
  third_party/PipeSDK/README.md
  src/
    main.cpp
    args/
    bridge/
    config/
    events/
    logging/
    model/
    pipe/
    util/
  tests/test_main.cpp
```

## Windows Build Requirements

```text
Windows 10/11
Visual Studio 2019 or newer with C++ workload
CMake 3.16+
Official Douyin Live Companion PipeSDK/demo files for real companion integration
```

## Build With Visual Studio / CMake

From repository root:

```powershell
cmake -S plugin-bridge-helper -B plugin-bridge-helper/build -G "Visual Studio 17 2022" -A x64
cmake --build plugin-bridge-helper/build --config Release
ctest --test-dir plugin-bridge-helper/build -C Release --output-on-failure
```

The executable will be under a Visual Studio configuration output directory such as:

```text
plugin-bridge-helper/build/Release/plugin-bridge-helper.exe
```

## Local Debug Without Live Companion

Start the existing overlay, enter edit mode, select `Bridge Receiver`, and set:

```text
ws://127.0.0.1:17891
```

Run helper in mock mode:

```powershell
plugin-bridge-helper.exe --mock --once pipeName=localPipe maxChannels=1 mateVersion=debug layoutMode=0 --config config/config.example.json
```

Without `--once`, the helper stays alive like a plugin process.

Mock mode emits system/comment/like/gift/fans_club/follow/total_like sample events when subscriptions are requested. This is only for local debug.

## Live Companion Debug Panel Flow

1. Start the existing Electron overlay.
2. Press `Ctrl+Alt+L` to enter edit mode.
3. Select data source `Bridge Receiver`.
4. Set bridge URL to `ws://127.0.0.1:17891`.
5. Build `plugin-bridge-helper.exe`.
6. Open Douyin Live Companion plugin debug panel.
7. Configure it to start `plugin-bridge-helper.exe`.
8. Let Live Companion pass `pipeName`, `maxChannels`, `mateVersion`, and `layoutMode`.
9. Enter the live test environment.
10. Use another account to send comment / like / gift.
11. Check the overlay for mapped live events.

## Config

Example:

```text
config/config.example.json
```

Important fields:

```json
{
  "bridge": {
    "url": "ws://127.0.0.1:17891",
    "reconnectMinMs": 500,
    "reconnectMaxMs": 10000
  },
  "logging": {
    "path": "logs/plugin-bridge-helper.log"
  },
  "capabilities": {
    "comment": true,
    "like": true,
    "gift": true,
    "fansClub": true,
    "follow": true,
    "enter": false,
    "totalLikeCount": true
  },
  "security": {
    "appSecretEnv": "DOUYIN_PLUGIN_APP_SECRET"
  }
}
```

Do not hardcode AppSecret. Set it in the environment if a future official flow requires it:

```powershell
setx DOUYIN_PLUGIN_APP_SECRET "your-secret"
```

## Event Mapping

```text
comment      -> type="comment"
like action  -> type="like" with likeCount
total like   -> type="like" with totalLikeCount and text="total_like_count"
gift         -> type="gift"
fans club    -> type="fans_club"
follow       -> type="follow" with followAction
enter        -> type="enter" only when capability enter=true and platform approval exists
unknown      -> dropped with system/error logging
```

All unconfirmed official fields should be kept in `rawJson` and not promoted to first-class payload fields until verified against official documentation.

## Troubleshooting

`pipeName` missing:

```text
Do not run normal official mode manually without companion args. Use --mock for local debug, or launch from Live Companion debug panel.
```

SDK initialization failed:

```text
Check third_party/PipeSDK contents and the PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK build flag.
Do not invent SDK function signatures. Use the official demo.
```

Subscription failed:

```text
Check whether the capability is approved in the official platform.
The helper should send a system event to overlay instead of crashing.
```

WebSocket cannot connect:

```text
Start the overlay first.
Set overlay data source to Bridge Receiver.
Check URL ws://127.0.0.1:17891 and Windows firewall rules.
```

User enter events missing:

```text
The enter capability is gated by config and disabled by default.
Enable only after official platform approval.
```

## Real Capability Status

Real after official SDK adapter is filled:

```text
comment / like / gift / fans club / follow / total like mapping and Bridge publish path
```

Requires official SDK/demo and platform approval:

```text
PipeSDK initialization and callbacks
actual official event subscription payloads
user enter data
any credential-backed request flow
```
