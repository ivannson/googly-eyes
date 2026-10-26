#include "radar_observation.h"

#include <algorithm>
#include <limits>

std::uint16_t nearestTargetDistanceCm(
    const bool movingDetected, const std::uint16_t movingDistanceCm,
    const bool stationaryDetected, const std::uint16_t stationaryDistanceCm) {
  const bool movingValid = movingDetected && movingDistanceCm != 0U;
  const bool stationaryValid =
      stationaryDetected && stationaryDistanceCm != 0U;

  if (movingValid && stationaryValid) {
    return std::min(movingDistanceCm, stationaryDistanceCm);
  }
  if (movingValid) {
    return movingDistanceCm;
  }
  if (stationaryValid) {
    return stationaryDistanceCm;
  }
  return 0U;
}

bool consumeRadarFrameActivity(std::uint32_t &pendingBytes,
                               const std::uint32_t newlyReceivedBytes,
                               const std::uint16_t validFrameLength) {
  const std::uint32_t availableRoom =
      std::numeric_limits<std::uint32_t>::max() - pendingBytes;
  pendingBytes += std::min(newlyReceivedBytes, availableRoom);
  if (validFrameLength == 0U || pendingBytes < validFrameLength) {
    return false;
  }
  pendingBytes = 0U;
  return true;
}
