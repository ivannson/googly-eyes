#pragma once

#include <cstdint>

struct RadarObservation {
  bool connected;
  bool presence;
  bool movingTarget;
  std::uint16_t distanceCm;
  std::uint32_t observedAtMs;
};

std::uint16_t nearestTargetDistanceCm(
    bool movingDetected, std::uint16_t movingDistanceCm,
    bool stationaryDetected, std::uint16_t stationaryDistanceCm);

bool consumeRadarFrameActivity(std::uint32_t &pendingBytes,
                               std::uint32_t newlyReceivedBytes,
                               std::uint16_t validFrameLength);
