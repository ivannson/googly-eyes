#include <cassert>
#include <cstdint>
#include <limits>

#include "frame_timing.h"

namespace {
void coordinatedFramesAreScheduledAtSixtyMilliseconds() {
  assert(!isEyeFrameDue(1059U, 1000U));
  assert(isEyeFrameDue(1060U, 1000U));
}

void schedulingSurvivesMillisRollover() {
  constexpr std::uint32_t last =
      std::numeric_limits<std::uint32_t>::max() - 29U;
  assert(!isEyeFrameDue(28U, last));
  assert(isEyeFrameDue(30U, last));
}
}  // namespace

int main() {
  coordinatedFramesAreScheduledAtSixtyMilliseconds();
  schedulingSurvivesMillisRollover();
  return 0;
}
