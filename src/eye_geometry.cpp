#include "eye_geometry.h"

PupilGeometry makeCenteredPupil(const std::uint16_t displayWidth,
                                const std::uint16_t displayHeight,
                                const std::uint16_t radius) {
  return {
      static_cast<std::uint16_t>(displayWidth / 2),
      static_cast<std::uint16_t>(displayHeight / 2),
      radius,
  };
}
