#include <cassert>

#include "display_orientation.h"

namespace {
void physicalMountingIsCompensated() {
  assert(displayRotation(EyeDisplay::Left) == 1U);
  assert(displayRotation(EyeDisplay::Right) == 3U);
}
}  // namespace

int main() {
  physicalMountingIsCompensated();
  return 0;
}
