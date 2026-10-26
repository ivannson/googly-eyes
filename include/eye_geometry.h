#pragma once

#include <cstdint>

struct PupilGeometry {
  std::uint16_t centerX;
  std::uint16_t centerY;
  std::uint16_t radius;
};

PupilGeometry makeCenteredPupil(std::uint16_t displayWidth,
                                std::uint16_t displayHeight,
                                std::uint16_t radius);
