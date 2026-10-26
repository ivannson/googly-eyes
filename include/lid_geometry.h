#pragma once

#include <cstdint>

#include "animation_engine.h"

struct LidGeometry {
  std::int16_t upperLeftY;
  std::int16_t upperRightY;
  std::int16_t lowerY;
};

LidGeometry makeLidGeometry(const EyePose &eye, float transitionClosure);
