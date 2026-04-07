cmake_minimum_required(VERSION 3.16)
project(plugin_bridge_helper VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK "Build with official PipeSDK integration" OFF)
set(_PLUGIN_BRIDGE_DEFAULT_PIPESDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/PipeSDK")
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/PipeSDK_FFmpeg/PipeDef.h")
  set(_PLUGIN_BRIDGE_DEFAULT_PIPESDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/PipeSDK_FFmpeg")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ConsoleApplication1/PipeDef.h")
  set(_PLUGIN_BRIDGE_DEFAULT_PIPESDK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/ConsoleApplication1")
endif()
set(PLUGIN_BRIDGE_PIPESDK_ROOT "${_PLUGIN_BRIDGE_DEFAULT_PIPESDK_ROOT}" CACHE PATH "Official PipeSDK root")

add_library(plugin_bridge_core
  src/args/launch_args.cpp
  src/bridge/envelope.cpp
  src/bridge/ws_publisher.cpp
  src/config/app_config.cpp
  src/events/event_mapper.cpp
  src/events/event_parser.cpp
  src/events/official_message_parser.cpp
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
  find_path(PIPESDK_INCLUDE_DIR
    NAMES PipeDef.h PipeSDK.h pipesdk.h PipeSDK/PipeSDK.h
    PATHS "${PLUGIN_BRIDGE_PIPESDK_ROOT}/include" "${PLUGIN_BRIDGE_PIPESDK_ROOT}"
    NO_DEFAULT_PATH
  )
  find_library(PIPESDK_LIBRARY
    NAMES PipeSDK pipesdk
    PATHS
      "${PLUGIN_BRIDGE_PIPESDK_ROOT}/lib"
      "${PLUGIN_BRIDGE_PIPESDK_ROOT}/lib/x64"
      "${PLUGIN_BRIDGE_PIPESDK_ROOT}/x64"
      "${PLUGIN_BRIDGE_PIPESDK_ROOT}"
    NO_DEFAULT_PATH
  )
  find_file(PIPESDK_DLL
    NAMES PipeSDK.dll pipesdk.dll
    PATHS
      "${PLUGIN_BRIDGE_PIPESDK_ROOT}/bin"
      "${PLUGIN_BRIDGE_PIPESDK_ROOT}/bin/x64"
      "${PLUGIN_BRIDGE_PIPESDK_ROOT}/lib"
      "${PLUGIN_BRIDGE_PIPESDK_ROOT}/lib/x64"
      "${PLUGIN_BRIDGE_PIPESDK_ROOT}/x64"
      "${PLUGIN_BRIDGE_PIPESDK_ROOT}"
    NO_DEFAULT_PATH
  )

  if(NOT PIPESDK_INCLUDE_DIR)
    message(FATAL_ERROR "Official PipeSDK headers not found. Put PipeDef.h or PipeSDK.h under ${PLUGIN_BRIDGE_PIPESDK_ROOT} or set PLUGIN_BRIDGE_PIPESDK_ROOT.")
  endif()
  if(NOT PIPESDK_LIBRARY)
    message(FATAL_ERROR "Official PipeSDK import library not found. Put PipeSDK.lib under ${PLUGIN_BRIDGE_PIPESDK_ROOT}/lib or set PLUGIN_BRIDGE_PIPESDK_ROOT.")
  endif()
  if(NOT PIPESDK_DLL)
    message(WARNING "Official PipeSDK.dll not found. The helper may build but must have PipeSDK.dll beside plugin-bridge-helper.exe at runtime.")
  endif()

  target_include_directories(plugin_bridge_core PUBLIC "${PIPESDK_INCLUDE_DIR}")
  target_link_libraries(plugin_bridge_core PUBLIC "${PIPESDK_LIBRARY}")
endif()

add_executable(plugin-bridge-helper src/main.cpp)
target_link_libraries(plugin-bridge-helper PRIVATE plugin_bridge_core)

if(PLUGIN_BRIDGE_WITH_OFFICIAL_PIPESDK AND PIPESDK_DLL)
  get_filename_component(PIPESDK_DLL_DIR "${PIPESDK_DLL}" DIRECTORY)
  file(GLOB PIPESDK_RUNTIME_DLLS "${PIPESDK_DLL_DIR}/*.dll")
  add_custom_command(TARGET plugin-bridge-helper POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${PIPESDK_RUNTIME_DLLS}
      "$<TARGET_FILE_DIR:plugin-bridge-helper>"
    COMMENT "Copying official PipeSDK runtime DLLs"
  )
endif()

add_executable(plugin_bridge_helper_tests
  tests/test_main.cpp
)
target_link_libraries(plugin_bridge_helper_tests PRIVATE plugin_bridge_core)

enable_testing()
add_test(NAME plugin_bridge_helper_tests COMMAND plugin_bridge_helper_tests)
