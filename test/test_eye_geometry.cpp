#include <cassert>
#include <cstdint>

#include "eye_geometry.h"

int main() {
  const PupilGeometry roundDisplay = makeCenteredPupil(240, 240, 45);
  assert(roundDisplay.centerX == 120);
  assert(roundDisplay.centerY == 120);
  assert(roundDisplay.radius == 45);

  const PupilGeometry rectangularDisplay = makeCenteredPupil(200, 100, 20);
  assert(rectangularDisplay.centerX == 100);
  assert(rectangularDisplay.centerY == 50);
  assert(rectangularDisplay.radius == 20);

  return 0;
}
