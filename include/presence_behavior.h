#pragma once

#include <cstdint>

#include "animation_engine.h"
#include "radar_observation.h"

enum class PresenceState : std::uint8_t {
  Sleeping,
  Waking,
  Normal,
  Cautious,
  Approaching,
  Hidden,
  Peeking,
  Retreating,
};

struct PresenceOutput {
  PresenceState state;
  AnimationProfile profile;
  float eyelidClosure;
  float pupilXOffset;
  bool becameVisible;
};

class PresenceBehavior {
 public:
  explicit PresenceBehavior(std::uint32_t seed);

  PresenceOutput update(const RadarObservation &observation,
                        std::uint32_t nowMs);

 private:
  enum class Trend : std::uint8_t { Stable, Approaching, Retreating };

  std::uint32_t nextRandom();
  void enterState(PresenceState state, std::uint32_t nowMs);
  void updateDistance(std::uint16_t distanceCm, std::uint32_t nowMs);
  PresenceOutput output(std::uint32_t nowMs, bool becameVisible) const;

  std::uint32_t randomState_;
  std::uint32_t stateEnteredAt_;
  std::uint32_t absenceStartedAt_;
  std::uint32_t trendReferenceAt_;
  std::uint32_t nextPeekDelayMs_;
  PresenceState state_;
  Trend trend_;
  float filteredDistanceCm_;
  float trendReferenceCm_;
  float peekOffset_;
  float stateEntryClosure_;
  std::uint8_t presentFrames_;
  std::uint8_t closeFrames_;
  bool absenceActive_;
  bool distanceInitialized_;
  bool trendInitialized_;
  bool observationSeen_;
  std::uint32_t lastObservationAt_;
};

void applyPresenceOverlay(EyePairPose &pose, const PresenceOutput &output);

const char *presenceStateName(PresenceState state);
