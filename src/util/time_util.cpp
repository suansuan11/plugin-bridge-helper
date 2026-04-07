#include "util/time_util.h"

#include <chrono>

namespace plugin_bridge {

int64_t NowMillis() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

}  // namespace plugin_bridge
