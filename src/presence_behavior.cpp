#include "presence_behavior.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr std::uint32_t kAbsenceDelayMs = 3000U;
constexpr std::uint32_t kObservationStaleMs = 1500U;
constexpr std::uint32_t kWakeDurationMs = 700U;
constexpr std::uint32_t kHideDurationMs = 350U;
constexpr std::uint32_t kPeekDurationMs = 700U;
constexpr std::uint32_t kTrendIntervalMs = 1000U;
constexpr float kDistanceAlpha = 0.65F;
constexpr float kTrendThresholdCm = 25.0F;
constexpr float kHideDistanceCm = 150.0F;
constexpr float kHideExitDistanceCm = 170.0F;
constexpr float kRetreatRehideDistanceCm = 135.0F;
constexpr float kCautiousDistanceCm = 300.0F;
constexpr float kNormalReturnDistanceCm = 320.0F;

float clamp01(const float value) {
  return std::max(0.0F, std::min(1.0F, value));
}

float smoothstep(const float value) {
  const float clamped = clamp01(value);
  return clamped * clamped * (3.0F - 2.0F * clamped);
}

AnimationProfile profileFor(const PresenceState state) {
  if (state == PresenceState::Normal) {
    return AnimationProfile::Normal;
  }
  if (state == PresenceState::Approaching) {
    return AnimationProfile::Approaching;
  }
  return AnimationProfile::Cautious;
}
}  // namespace

PresenceBehavior::PresenceBehavior(const std::uint32_t seed)
    : randomState_(seed == 0U ? 0xA341316CU : seed),
      stateEnteredAt_(0U),
      absenceStartedAt_(0U),
      trendReferenceAt_(0U),
      nextPeekDelayMs_(3000U),
      state_(PresenceState::Sleeping),
      trend_(Trend::Stable),
      filteredDistanceCm_(0.0F),
      trendReferenceCm_(0.0F),
      peekOffset_(0.0F),
      stateEntryClosure_(1.0F),
      presentFrames_(0U),
      closeFrames_(0U),
      absenceActive_(false),
      distanceInitialized_(false),
      trendInitialized_(false),
      observationSeen_(false),
      lastObservationAt_(0U) {}

PresenceOutput PresenceBehavior::update(const RadarObservation &observation,
                                        const std::uint32_t nowMs) {
  bool becameVisible = false;
  const bool newObservation =
      !observationSeen_ || observation.observedAtMs != lastObservationAt_;
  if (newObservation) {
    observationSeen_ = true;
    lastObservationAt_ = observation.observedAtMs;
  }
  const std::uint32_t observationAge = nowMs - observation.observedAtMs;
  const bool fresh = observation.connected &&
                     observationAge <= kObservationStaleMs;
  const bool present = fresh && observation.presence;

  if (!present) {
    presentFrames_ = 0U;
    closeFrames_ = 0U;
    if (!absenceActive_) {
      absenceActive_ = true;
      if (!observation.connected || fresh) {
        absenceStartedAt_ = nowMs;
      } else {
        absenceStartedAt_ = observation.observedAtMs;
        absenceStartedAt_ += kObservationStaleMs;
      }
    }
    if (nowMs - absenceStartedAt_ >= kAbsenceDelayMs) {
      if (state_ != PresenceState::Sleeping) {
        enterState(PresenceState::Sleeping, nowMs);
      }
      return output(nowMs, false);
    }
    return output(nowMs, false);
  }

  absenceActive_ = false;
  if (newObservation && presentFrames_ < 2U) {
    ++presentFrames_;
  }
  const bool validDistance = newObservation && observation.distanceCm != 0U;
  if (validDistance) {
    updateDistance(observation.distanceCm, nowMs);
  } else if (newObservation) {
    closeFrames_ = 0U;
  }

  if (state_ == PresenceState::Sleeping) {
    if (presentFrames_ >= 2U) {
      enterState(PresenceState::Waking, nowMs);
      becameVisible = true;
    }
    return output(nowMs, becameVisible);
  }

  if (state_ == PresenceState::Waking) {
    if (nowMs - stateEnteredAt_ < kWakeDurationMs) {
      return output(nowMs, false);
    }
    if (distanceInitialized_ && filteredDistanceCm_ <= kCautiousDistanceCm) {
      enterState(PresenceState::Cautious, nowMs);
    } else {
      enterState(PresenceState::Normal, nowMs);
    }
  }

  if (state_ == PresenceState::Hidden || state_ == PresenceState::Peeking) {
    if (validDistance && distanceInitialized_ &&
        (trend_ == Trend::Retreating ||
         filteredDistanceCm_ > kHideExitDistanceCm)) {
      enterState(PresenceState::Retreating, nowMs);
      closeFrames_ = 0U;
      becameVisible = true;
      return output(nowMs, becameVisible);
    }

    if (state_ == PresenceState::Peeking) {
      if (nowMs - stateEnteredAt_ >= kPeekDurationMs) {
        enterState(PresenceState::Hidden, nowMs);
        nextPeekDelayMs_ = 3000U + nextRandom() % 2001U;
      }
      return output(nowMs, false);
    }

    if (nowMs - stateEnteredAt_ >= nextPeekDelayMs_) {
      enterState(PresenceState::Peeking, nowMs);
      peekOffset_ = (nextRandom() & 1U) == 0U ? -16.0F : 16.0F;
    }
    return output(nowMs, false);
  }

  if (state_ == PresenceState::Retreating) {
    if (validDistance && distanceInitialized_ &&
        trend_ == Trend::Approaching &&
        filteredDistanceCm_ < kRetreatRehideDistanceCm) {
      if (closeFrames_ < 2U) {
        ++closeFrames_;
      }
      if (closeFrames_ >= 2U) {
        enterState(PresenceState::Hidden, nowMs);
        nextPeekDelayMs_ = 3000U + nextRandom() % 2001U;
      }
    } else if (validDistance) {
      closeFrames_ = 0U;
      if (distanceInitialized_ &&
          filteredDistanceCm_ > kNormalReturnDistanceCm) {
        enterState(PresenceState::Normal, nowMs);
      }
    }
    return output(nowMs, false);
  }

  if (validDistance && distanceInitialized_ &&
      filteredDistanceCm_ < kHideDistanceCm) {
    if (closeFrames_ < 2U) {
      ++closeFrames_;
    }
    if (closeFrames_ >= 2U) {
      enterState(PresenceState::Hidden, nowMs);
      closeFrames_ = 0U;
      nextPeekDelayMs_ = 3000U + nextRandom() % 2001U;
      return output(nowMs, false);
    }
  } else if (newObservation) {
    closeFrames_ = 0U;
  }

  if (validDistance && distanceInitialized_) {
    if (trend_ == Trend::Approaching &&
        filteredDistanceCm_ <= kCautiousDistanceCm) {
      enterState(PresenceState::Approaching, nowMs);
    } else if (filteredDistanceCm_ > kNormalReturnDistanceCm) {
      enterState(PresenceState::Normal, nowMs);
    } else if (filteredDistanceCm_ <= kCautiousDistanceCm ||
               state_ != PresenceState::Normal) {
      enterState(PresenceState::Cautious, nowMs);
    }
  }

  return output(nowMs, false);
}

void PresenceBehavior::updateDistance(const std::uint16_t distanceCm,
                                      const std::uint32_t nowMs) {
  if (!distanceInitialized_) {
    filteredDistanceCm_ = static_cast<float>(distanceCm);
    distanceInitialized_ = true;
  } else {
    filteredDistanceCm_ +=
        (static_cast<float>(distanceCm) - filteredDistanceCm_) *
        kDistanceAlpha;
  }

  if (!trendInitialized_) {
    trendReferenceCm_ = filteredDistanceCm_;
    trendReferenceAt_ = nowMs;
    trendInitialized_ = true;
    return;
  }

  if (nowMs - trendReferenceAt_ < kTrendIntervalMs) {
    return;
  }

  const float change = filteredDistanceCm_ - trendReferenceCm_;
  if (change <= -kTrendThresholdCm) {
    trend_ = Trend::Approaching;
  } else if (change >= kTrendThresholdCm) {
    trend_ = Trend::Retreating;
  } else {
    trend_ = Trend::Stable;
  }
  trendReferenceCm_ = filteredDistanceCm_;
  trendReferenceAt_ = nowMs;
}

PresenceOutput PresenceBehavior::output(const std::uint32_t nowMs,
                                        const bool becameVisible) const {
  float closure = 0.0F;
  float pupilOffset = 0.0F;
  const std::uint32_t elapsed = nowMs - stateEnteredAt_;

  switch (state_) {
    case PresenceState::Sleeping:
      closure = 1.0F;
      break;
    case PresenceState::Waking:
      closure = stateEntryClosure_ *
                (1.0F - smoothstep(static_cast<float>(elapsed) /
                                   static_cast<float>(kWakeDurationMs)));
      break;
    case PresenceState::Hidden:
      closure = stateEntryClosure_ +
                (1.0F - stateEntryClosure_) *
                    smoothstep(static_cast<float>(elapsed) /
                               static_cast<float>(kHideDurationMs));
      break;
    case PresenceState::Peeking:
      closure = 0.72F;
      pupilOffset = peekOffset_;
      break;
    case PresenceState::Retreating:
      closure = stateEntryClosure_ *
                (1.0F - smoothstep(static_cast<float>(elapsed) /
                                   static_cast<float>(kWakeDurationMs)));
      break;
    case PresenceState::Normal:
    case PresenceState::Cautious:
    case PresenceState::Approaching:
      break;
  }

  return {state_, profileFor(state_), closure, pupilOffset, becameVisible};
}

void PresenceBehavior::enterState(const PresenceState state,
                                  const std::uint32_t nowMs) {
  if (state_ == state) {
    return;
  }
  const float previousClosure = output(nowMs, false).eyelidClosure;
  state_ = state;
  stateEnteredAt_ = nowMs;
  stateEntryClosure_ = previousClosure;
  if (state == PresenceState::Sleeping) {
    closeFrames_ = 0U;
    distanceInitialized_ = false;
    trendInitialized_ = false;
    trend_ = Trend::Stable;
    filteredDistanceCm_ = 0.0F;
    trendReferenceCm_ = 0.0F;
  }
}

std::uint32_t PresenceBehavior::nextRandom() {
  randomState_ ^= randomState_ << 13U;
  randomState_ ^= randomState_ >> 17U;
  randomState_ ^= randomState_ << 5U;
  return randomState_;
}

void applyPresenceOverlay(EyePairPose &pose, const PresenceOutput &output) {
  pose.transitionClosure =
      std::max(pose.transitionClosure, clamp01(output.eyelidClosure));

  const auto offsetPupil = [&output](EyePose &eye) {
    const float minimum = eye.pupilRadius;
    const float maximum = 240.0F - eye.pupilRadius;
    eye.pupilX = std::max(
        minimum, std::min(maximum, eye.pupilX + output.pupilXOffset));
  };
  offsetPupil(pose.left);
  offsetPupil(pose.right);
}

const char *presenceStateName(const PresenceState state) {
  switch (state) {
    case PresenceState::Sleeping:
      return "sleeping";
    case PresenceState::Waking:
      return "waking";
    case PresenceState::Normal:
      return "normal";
    case PresenceState::Cautious:
      return "cautious";
    case PresenceState::Approaching:
      return "approaching";
    case PresenceState::Hidden:
      return "hidden";
    case PresenceState::Peeking:
      return "peeking";
    case PresenceState::Retreating:
      return "retreating";
  }
  return "unknown";
}
