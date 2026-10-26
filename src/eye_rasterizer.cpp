#include "eye_rasterizer.h"

#include <cmath>

#include "lid_geometry.h"

namespace {
struct RasterState {
  LidGeometry lids;
  std::int32_t pupilX;
  std::int32_t pupilY;
  std::int32_t radiusSquared;
  std::int32_t slantHeight;
};

RasterState makeRasterState(const EyePose &eye, const float closure) {
  const LidGeometry lids = makeLidGeometry(eye, closure);
  const std::int32_t radius =
      static_cast<std::int32_t>(std::lround(eye.pupilRadius));
  return {
      lids,
      static_cast<std::int32_t>(std::lround(eye.pupilX)),
      static_cast<std::int32_t>(std::lround(eye.pupilY)),
      radius * radius,
      lids.upperRightY - lids.upperLeftY,
  };
}

bool isBlackAt(const RasterState &state, const std::int32_t x,
               const std::int32_t y) {
  const std::int32_t upperBoundary =
      state.lids.upperLeftY + state.slantHeight * x / (kEyeWidth - 1);
  const std::int32_t dx = x - state.pupilX;
  const std::int32_t dy = y - state.pupilY;
  const bool insidePupil = dx * dx + dy * dy <= state.radiusSquared;
  const bool behindUpperLid = upperBoundary > 0 && y < upperBoundary;
  const bool behindLowerLid =
      state.lids.lowerY < kEyeHeight && y >= state.lids.lowerY;
  return insidePupil || behindUpperLid || behindLowerLid;
}
}  // namespace

// Compose the whole eye in RAM so the display receives only final pixels.
void composeEyeFrame(const EyePose &eye, const float transitionClosure,
                     std::uint16_t *const pixels) {
  const RasterState state = makeRasterState(eye, transitionClosure);
  for (std::int32_t y = 0; y < kEyeHeight; ++y) {
    for (std::int32_t x = 0; x < kEyeWidth; ++x) {
      pixels[static_cast<std::size_t>(y) * kEyeWidth + x] =
          isBlackAt(state, x, y) ? kEyeBlack : kEyeWhite;
    }
  }
}
