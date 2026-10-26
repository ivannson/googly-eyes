#include <cassert>

#include "radar_observation.h"

namespace {
void nearerMovingTargetWins() {
  assert(nearestTargetDistanceCm(true, 220U, true, 410U) == 220U);
}

void nearerStationaryTargetWins() {
  assert(nearestTargetDistanceCm(true, 450U, true, 180U) == 180U);
}

void zeroDistancesAreIgnored() {
  assert(nearestTargetDistanceCm(true, 0U, true, 240U) == 240U);
  assert(nearestTargetDistanceCm(true, 330U, true, 0U) == 330U);
}

void noValidDistanceReturnsUnknown() {
  assert(nearestTargetDistanceCm(false, 0U, false, 0U) == 0U);
  assert(nearestTargetDistanceCm(true, 0U, true, 0U) == 0U);
}

void cachedValuesDoNotLookLikeNewFrames() {
  std::uint32_t pendingBytes = 0U;
  assert(!consumeRadarFrameActivity(pendingBytes, 0U, 23U));
  assert(pendingBytes == 0U);
}

void partialFrameBytesAccumulateUntilComplete() {
  std::uint32_t pendingBytes = 0U;
  assert(!consumeRadarFrameActivity(pendingBytes, 10U, 23U));
  assert(pendingBytes == 10U);
  assert(consumeRadarFrameActivity(pendingBytes, 13U, 23U));
  assert(pendingBytes == 0U);
}

void queuedFramesProduceOneFreshLatestObservation() {
  std::uint32_t pendingBytes = 0U;
  assert(consumeRadarFrameActivity(pendingBytes, 46U, 23U));
  assert(pendingBytes == 0U);
  assert(!consumeRadarFrameActivity(pendingBytes, 0U, 23U));
}
}  // namespace

int main() {
  nearerMovingTargetWins();
  nearerStationaryTargetWins();
  zeroDistancesAreIgnored();
  noValidDistanceReturnsUnknown();
  cachedValuesDoNotLookLikeNewFrames();
  partialFrameBytesAccumulateUntilComplete();
  queuedFramesProduceOneFreshLatestObservation();
  return 0;
}
