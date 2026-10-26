#include <array>
#include <cassert>
#include <cstddef>

#include "eye_rasterizer.h"

namespace {
std::size_t pixelIndex(const std::uint16_t x, const std::uint16_t y) {
  return static_cast<std::size_t>(y) * kEyeWidth + x;
}

void openEyeComposesBackgroundAndPupilBeforeTransfer() {
  std::array<std::uint16_t, kEyePixelCount> pixels{};
  const EyePose neutral = {120.0F, 120.0F, 45.0F, 0.0F, 0.0F, 0.0F};
  composeEyeFrame(neutral, 0.0F, pixels.data());

  assert(pixels[pixelIndex(0, 0)] == kEyeWhite);
  assert(pixels[pixelIndex(120, 120)] == kEyeBlack);
  assert(pixels[pixelIndex(120, 74)] == kEyeWhite);
}

void fullBlinkComposesAnEntirelyBlackFrame() {
  std::array<std::uint16_t, kEyePixelCount> pixels{};
  const EyePose neutral = {120.0F, 120.0F, 45.0F, 0.0F, 0.0F, 0.0F};
  composeEyeFrame(neutral, 1.0F, pixels.data());

  for (const std::uint16_t pixel : pixels) {
    assert(pixel == kEyeBlack);
  }
}

void slantedUpperLidIsComposedIntoTheFinalPixels() {
  std::array<std::uint16_t, kEyePixelCount> pixels{};
  const EyePose angry = {120.0F, 120.0F, 45.0F, 0.3F, 0.0F, 30.0F};
  composeEyeFrame(angry, 0.0F, pixels.data());

  assert(pixels[pixelIndex(0, 20)] == kEyeBlack);
  assert(pixels[pixelIndex(0, 22)] == kEyeWhite);
  assert(pixels[pixelIndex(239, 50)] == kEyeBlack);
  assert(pixels[pixelIndex(239, 52)] == kEyeWhite);
}

}  // namespace

int main() {
  openEyeComposesBackgroundAndPupilBeforeTransfer();
  fullBlinkComposesAnEntirelyBlackFrame();
  slantedUpperLidIsComposedIntoTheFinalPixels();
  return 0;
}
