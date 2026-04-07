#include "util/uuid.h"

#include <iomanip>
#include <random>
#include <sstream>

namespace plugin_bridge {

std::string GenerateUuid() {
  static thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<unsigned long long> dist;
  std::ostringstream out;
  out << std::hex << std::setfill('0')
      << std::setw(16) << dist(rng)
      << std::setw(16) << dist(rng);
  return out.str();
}

}  // namespace plugin_bridge
