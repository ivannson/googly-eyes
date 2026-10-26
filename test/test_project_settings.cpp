#include <cassert>

#include "project_settings.h"

namespace {
void disabledMmWaveUsesAutonomousAnimationMode() {
  const EyeControlPolicy policy = eyeControlPolicy();
  assert(!policy.initializeRadar);
  assert(!policy.pollRadar);
  assert(!policy.applyPresenceBehavior);
}
}  // namespace

int main() {
  disabledMmWaveUsesAutonomousAnimationMode();
  return 0;
}
