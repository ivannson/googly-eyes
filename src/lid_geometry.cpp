#include "lid_geometry.h"

#include <algorithm>
#include <cmath>

namespace {
float clamp01(const float value) {
  return std::max(0.0F, std::min(1.0F, value));
}

std::int16_t displayCoordinate(const float value) {
  return static_cast<std::int16_t>(
      std::lround(std::max(0.0F, std::min(240.0F, value))));
}
}  // namespace

LidGeometry makeLidGeometry(const EyePose &eye,
                            const float transitionClosure) {
  const float transition = clamp01(transitionClosure);
  const float upperClosure =
      std::max(clamp01(eye.upperLidClosure), transition);
  const float lowerClosure =
      std::max(clamp01(eye.lowerLidClosure), transition);
  const float slant = eye.upperLidSlant * (1.0F - transition);
  const float upperCenter = upperClosure * 120.0F;

  return {
      displayCoordinate(upperCenter - slant * 0.5F),
      displayCoordinate(upperCenter + slant * 0.5F),
      displayCoordinate(240.0F - lowerClosure * 120.0F),
  };
}
