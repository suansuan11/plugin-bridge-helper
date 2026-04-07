cmake_minimum_required(VERSION 3.16)
project(plugin_bridge_helper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK "Build with official PipeSDK integration" OFF)

add_library(plugin_bridge_core
  src/args/launch_args.cpp
  src/bridge/envelope.cpp
  src/bridge/ws_publisher.cpp
  src/config/app_config.cpp
  src/events/event_mapper.cpp
  src/events/event_parser.cpp
  src/logging/logger.cpp
  src/model/live_event.cpp
  src/pipe/pipe_client.cpp
  src/pipe/request_client.cpp
  src/util/time_util.cpp
  src/util/uuid.cpp
)

target_include_directories(plugin_bridge_core PUBLIC src)

if(WIN32)
  target_link_libraries(plugin_bridge_core PUBLIC winhttp)
  target_compile_definitions(plugin_bridge_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if(PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK)
  target_compile_definitions(plugin_bridge_core PUBLIC PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK=1)
  target_include_directories(plugin_bridge_core PUBLIC third_party/PipeSDK/include)
endif()

add_executable(plugin-bridge-helper src/main.cpp)
target_link_libraries(plugin-bridge-helper PRIVATE plugin_bridge_core)

add_executable(plugin_bridge_helper_tests
  tests/test_main.cpp
)
target_link_libraries(plugin_bridge_helper_tests PRIVATE plugin_bridge_core)

enable_testing()
add_test(NAME plugin_bridge_helper_tests COMMAND plugin_bridge_helper_tests)
