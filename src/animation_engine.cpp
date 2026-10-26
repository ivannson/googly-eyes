#include "animation_engine.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846F;
constexpr float kTau = 2.0F * kPi;
constexpr std::uint32_t kBlinkCloseMs = 180U;
constexpr std::uint32_t kBlinkOpenMs = 220U;
constexpr std::uint32_t kSmoothTransitionMs = 320U;
constexpr std::uint8_t kNormalWeights[kAnimationCount] = {
    18U,  // IdleWander
    8U,   // QuickGlance
    18U,  // SingleBlink
    10U,  // DoubleBlink
    10U,  // IndependentBlink
    16U,  // SuspiciousSquint
    5U,   // AngryGlare
    5U,   // Surprised
    4U,   // CrossEyed
    2U,   // DizzyOrbit
    3U,   // SleepWake
    1U,   // CrazyEyes
    6U,   // TrackingSweep
};
constexpr std::uint8_t kCautiousWeights[kAnimationCount] = {
    14U, 5U, 20U, 10U, 10U, 32U, 8U,
    3U,  2U, 0U,  2U,  0U,  4U,
};
constexpr std::uint8_t kApproachingWeights[kAnimationCount] = {
    6U, 3U, 18U, 8U, 8U, 38U, 32U,
    2U, 1U, 0U,  1U, 0U, 3U,
};

const std::uint8_t *weightsFor(const AnimationProfile profile) {
  switch (profile) {
    case AnimationProfile::Normal:
      return kNormalWeights;
    case AnimationProfile::Cautious:
      return kCautiousWeights;
    case AnimationProfile::Approaching:
      return kApproachingWeights;
  }
  return kNormalWeights;
}

constexpr EyePose kNeutralEye = {120.0F, 120.0F, 45.0F, 0.0F, 0.0F, 0.0F};

float clamp01(const float value) {
  return std::max(0.0F, std::min(1.0F, value));
}

float smoothstep(const float value) {
  const float clamped = clamp01(value);
  return clamped * clamped * (3.0F - 2.0F * clamped);
}

float easeBetween(const std::uint32_t elapsedMs, const std::uint32_t startMs,
                  const std::uint32_t endMs) {
  if (elapsedMs <= startMs) {
    return 0.0F;
  }
  if (elapsedMs >= endMs || endMs == startMs) {
    return 1.0F;
  }
  return smoothstep(static_cast<float>(elapsedMs - startMs) /
                    static_cast<float>(endMs - startMs));
}

float activeEnvelope(const std::uint32_t elapsedMs,
                     const std::uint32_t enterEndMs,
                     const std::uint32_t exitStartMs,
                     const std::uint32_t exitEndMs) {
  if (elapsedMs < enterEndMs) {
    return easeBetween(elapsedMs, 0U, enterEndMs);
  }
  if (elapsedMs <= exitStartMs) {
    return 1.0F;
  }
  return 1.0F - easeBetween(elapsedMs, exitStartMs, exitEndMs);
}

float blinkPulse(const std::uint32_t elapsedMs, const std::uint32_t startMs,
                 const std::uint32_t durationMs) {
  if (elapsedMs <= startMs || elapsedMs >= startMs + durationMs) {
    return 0.0F;
  }
  const std::uint32_t local = elapsedMs - startMs;
  const std::uint32_t half = durationMs / 2U;
  if (local <= half) {
    return easeBetween(local, 0U, half);
  }
  return 1.0F - easeBetween(local, half, durationMs);
}

std::uint32_t mixBits(std::uint32_t value) {
  value ^= value >> 16U;
  value *= 0x7FEB352DU;
  value ^= value >> 15U;
  value *= 0x846CA68BU;
  value ^= value >> 16U;
  return value;
}

float signedFromBits(const std::uint32_t value, const float magnitude) {
  const float normalized =
      static_cast<float>(value & 0xFFFFU) / static_cast<float>(0xFFFFU);
  return (normalized * 2.0F - 1.0F) * magnitude;
}

EyePairPose neutralPair(const AnimationId animation) {
  return {kNeutralEye, kNeutralEye, animation, 0.0F, false};
}

EyePose interpolateEye(const EyePose &from, const EyePose &to,
                       const float amount) {
  const auto interpolate = [amount](const float start, const float end) {
    return start + (end - start) * amount;
  };
  return {
      interpolate(from.pupilX, to.pupilX),
      interpolate(from.pupilY, to.pupilY),
      interpolate(from.pupilRadius, to.pupilRadius),
      interpolate(from.upperLidClosure, to.upperLidClosure),
      interpolate(from.lowerLidClosure, to.lowerLidClosure),
      interpolate(from.upperLidSlant, to.upperLidSlant),
  };
}

EyePairPose interpolatePair(const EyePairPose &from, const EyePairPose &to,
                            const float amount) {
  return {
      interpolateEye(from.left, to.left, amount),
      interpolateEye(from.right, to.right, amount),
      to.animation,
      0.0F,
      true,
  };
}

void applyBlink(EyePose &eye, const float closure) {
  eye.upperLidClosure = std::max(eye.upperLidClosure, closure);
  eye.lowerLidClosure = std::max(eye.lowerLidClosure, closure);
}
}  // namespace

bool isBlinkAnimation(const AnimationId animation) {
  return animation == AnimationId::SingleBlink ||
         animation == AnimationId::DoubleBlink ||
         animation == AnimationId::IndependentBlink;
}

AnimationId chooseNextAnimation(const AnimationId current,
                                const std::uint32_t randomValue,
                                const AnimationProfile profile) {
  const std::uint8_t *const weights = weightsFor(profile);
  const std::uint8_t currentIndex = static_cast<std::uint8_t>(current);
  std::uint32_t availableWeight = 0U;
  for (std::uint8_t index = 0U; index < kAnimationCount; ++index) {
    if (index != currentIndex) {
      availableWeight += weights[index];
    }
  }

  std::uint32_t ticket = randomValue % availableWeight;
  for (std::uint8_t index = 0U; index < kAnimationCount; ++index) {
    if (index == currentIndex) {
      continue;
    }
    if (ticket < weights[index]) {
      return static_cast<AnimationId>(index);
    }
    ticket -= weights[index];
  }

  return current == AnimationId::IdleWander ? AnimationId::SingleBlink
                                             : AnimationId::IdleWander;
}

bool usesBlinkTransition(const AnimationId current, const AnimationId next) {
  return !isBlinkAnimation(current) && !isBlinkAnimation(next);
}

std::uint32_t animationDurationMs(const AnimationId animation) {
  switch (animation) {
    case AnimationId::IdleWander:
      return 6500U;
    case AnimationId::QuickGlance:
      return 3200U;
    case AnimationId::SingleBlink:
      return 2600U;
    case AnimationId::DoubleBlink:
      return 3200U;
    case AnimationId::IndependentBlink:
      return 3400U;
    case AnimationId::SuspiciousSquint:
      return 5600U;
    case AnimationId::AngryGlare:
      return 4200U;
    case AnimationId::Surprised:
      return 3500U;
    case AnimationId::CrossEyed:
      return 3800U;
    case AnimationId::DizzyOrbit:
      return 2400U;
    case AnimationId::SleepWake:
      return 6000U;
    case AnimationId::CrazyEyes:
      return 1500U;
    case AnimationId::TrackingSweep:
      return 5200U;
    case AnimationId::Count:
      break;
  }
  return 2000U;
}

EyePairPose sampleAnimation(const AnimationId animation,
                            const std::uint32_t elapsedMs,
                            const std::uint32_t variation) {
  EyePairPose frame = neutralPair(animation);

  switch (animation) {
    case AnimationId::IdleWander: {
      const float phase = static_cast<float>(variation & 0xFFU) / 255.0F * kTau;
      const float seconds = static_cast<float>(elapsedMs) / 1000.0F;
      const float envelope = activeEnvelope(elapsedMs, 450U, 5800U, 6500U);
      const float x = std::sin(seconds * 1.15F + phase) * 32.0F * envelope;
      const float y = std::sin(seconds * 0.73F + phase * 0.41F) * 22.0F * envelope;
      frame.left.pupilX += x;
      frame.right.pupilX += x;
      frame.left.pupilY += y;
      frame.right.pupilY += y;
      break;
    }

    case AnimationId::QuickGlance: {
      const std::uint8_t direction = static_cast<std::uint8_t>(variation % 4U);
      const float xTarget = direction == 0U ? -42.0F : direction == 1U ? 42.0F : 0.0F;
      const float yTarget = direction == 2U ? -34.0F : direction == 3U ? 34.0F : 0.0F;
      const float envelope = activeEnvelope(elapsedMs, 250U, 2400U, 3200U);
      frame.left.pupilX += xTarget * envelope;
      frame.right.pupilX += xTarget * envelope;
      frame.left.pupilY += yTarget * envelope;
      frame.right.pupilY += yTarget * envelope;
      break;
    }

    case AnimationId::SingleBlink: {
      const float closure = blinkPulse(elapsedMs, 600U, 320U);
      applyBlink(frame.left, closure);
      applyBlink(frame.right, closure);
      break;
    }

    case AnimationId::DoubleBlink: {
      const float closure =
          std::max(blinkPulse(elapsedMs, 430U, 260U),
                   blinkPulse(elapsedMs, 900U, 260U));
      applyBlink(frame.left, closure);
      applyBlink(frame.right, closure);
      break;
    }

    case AnimationId::IndependentBlink: {
      applyBlink(frame.left, blinkPulse(elapsedMs, 450U, 300U));
      applyBlink(frame.right, blinkPulse(elapsedMs, 680U, 300U));
      break;
    }

    case AnimationId::SuspiciousSquint: {
      const float envelope = activeEnvelope(elapsedMs, 700U, 4700U, 5600U);
      const float direction = (variation & 1U) == 0U ? -1.0F : 1.0F;
      frame.left.pupilX += direction * 32.0F * envelope;
      frame.right.pupilX += direction * 32.0F * envelope;
      frame.left.upperLidClosure = 0.70F * envelope;
      frame.right.upperLidClosure = 0.70F * envelope;
      frame.left.lowerLidClosure = 0.50F * envelope;
      frame.right.lowerLidClosure = 0.50F * envelope;
      break;
    }

    case AnimationId::AngryGlare: {
      const float envelope = activeEnvelope(elapsedMs, 600U, 3300U, 4200U);
      frame.left.pupilX += 16.0F * envelope;
      frame.right.pupilX -= 16.0F * envelope;
      frame.left.pupilY += 9.0F * envelope;
      frame.right.pupilY += 9.0F * envelope;
      frame.left.upperLidClosure = 0.78F * envelope;
      frame.right.upperLidClosure = 0.78F * envelope;
      frame.left.lowerLidClosure = 0.42F * envelope;
      frame.right.lowerLidClosure = 0.42F * envelope;
      frame.left.upperLidSlant = 30.0F * envelope;
      frame.right.upperLidSlant = -30.0F * envelope;
      break;
    }

    case AnimationId::Surprised: {
      const float envelope = activeEnvelope(elapsedMs, 450U, 2700U, 3500U);
      frame.left.pupilRadius -= 27.0F * envelope;
      frame.right.pupilRadius -= 27.0F * envelope;
      break;
    }

    case AnimationId::CrossEyed: {
      const float envelope = activeEnvelope(elapsedMs, 600U, 2900U, 3800U);
      frame.left.pupilX += 35.0F * envelope;
      frame.right.pupilX -= 35.0F * envelope;
      break;
    }

    case AnimationId::DizzyOrbit: {
      const float envelope = activeEnvelope(elapsedMs, 300U, 1900U, 2400U);
      const float angle = static_cast<float>(elapsedMs) / 1050.0F * kTau;
      const float x = std::cos(angle) * 36.0F * envelope;
      const float y = std::sin(angle) * 30.0F * envelope;
      frame.left.pupilX += x;
      frame.left.pupilY += y;
      frame.right.pupilX -= x;
      frame.right.pupilY -= y;
      break;
    }

    case AnimationId::SleepWake: {
      float closure = 0.0F;
      if (elapsedMs < 1800U) {
        closure = easeBetween(elapsedMs, 800U, 1800U);
      } else if (elapsedMs <= 4200U) {
        closure = 1.0F;
      } else {
        closure = 1.0F - easeBetween(elapsedMs, 4200U, 5400U);
      }
      applyBlink(frame.left, closure);
      applyBlink(frame.right, closure);
      break;
    }

    case AnimationId::CrazyEyes: {
      const float envelope = activeEnvelope(elapsedMs, 150U, 1200U, 1500U);
      const std::uint32_t segment = elapsedMs / 120U;
      const std::uint32_t leftBits = mixBits(variation ^ (segment * 0x9E3779B9U));
      const std::uint32_t rightBits = mixBits(leftBits ^ 0xA5A5A5A5U);
      frame.left.pupilX += signedFromBits(leftBits, 42.0F) * envelope;
      frame.left.pupilY += signedFromBits(leftBits >> 16U, 32.0F) * envelope;
      frame.right.pupilX += signedFromBits(rightBits, 42.0F) * envelope;
      frame.right.pupilY += signedFromBits(rightBits >> 16U, 32.0F) * envelope;
      frame.left.pupilRadius = 34.0F + 11.0F * (1.0F - envelope);
      frame.right.pupilRadius = frame.left.pupilRadius;
      break;
    }

    case AnimationId::TrackingSweep: {
      const float envelope = activeEnvelope(elapsedMs, 500U, 4300U, 5200U);
      const float angle = static_cast<float>(elapsedMs) / 2400.0F * kTau;
      const float x = std::cos(angle) * 42.0F * envelope;
      const float y = std::sin(angle * 2.0F) * 28.0F * envelope;
      frame.left.pupilX += x;
      frame.right.pupilX += x;
      frame.left.pupilY += y;
      frame.right.pupilY += y;
      break;
    }

    case AnimationId::Count:
      break;
  }

  return frame;
}

AnimationEngine::AnimationEngine(const std::uint32_t seed)
    : randomState_(seed == 0U ? 0x6D2B79F5U : seed),
      phaseStartedAt_(0U),
      animationStartedAt_(0U),
      animationVariation_(seed),
      pendingVariation_(seed),
      current_(AnimationId::IdleWander),
      pending_(AnimationId::IdleWander),
      phase_(Phase::Playing),
      profile_(AnimationProfile::Normal),
      started_(false),
      transitionFrom_(sampleAnimation(AnimationId::IdleWander, 0U, seed)) {}

EyePairPose AnimationEngine::update(const std::uint32_t nowMs) {
  if (!started_) {
    started_ = true;
    phaseStartedAt_ = nowMs;
    animationStartedAt_ = nowMs;
  }

  if (phase_ == Phase::Playing) {
    const std::uint32_t elapsed = nowMs - animationStartedAt_;
    const std::uint32_t duration = animationDurationMs(current_);
    if (elapsed < duration) {
      return sampleAnimation(current_, elapsed, animationVariation_);
    }

    transitionFrom_ = sampleAnimation(current_, duration, animationVariation_);
    pending_ = chooseNextAnimation(current_, nextRandom(), profile_);
    pendingVariation_ = nextRandom();
    phaseStartedAt_ = nowMs;
    phase_ = usesBlinkTransition(current_, pending_)
                 ? Phase::BlinkClosing
                 : Phase::SmoothTransition;
    transitionFrom_.transitioning = true;
    return transitionFrom_;
  }

  if (phase_ == Phase::BlinkClosing) {
    const std::uint32_t elapsed = nowMs - phaseStartedAt_;
    if (elapsed < kBlinkCloseMs) {
      EyePairPose frame = transitionFrom_;
      frame.transitionClosure =
          smoothstep(static_cast<float>(elapsed) /
                     static_cast<float>(kBlinkCloseMs));
      frame.transitioning = true;
      return frame;
    }

    current_ = pending_;
    animationVariation_ = pendingVariation_;
    animationStartedAt_ = nowMs;
    phaseStartedAt_ = nowMs;
    phase_ = Phase::BlinkOpening;
    EyePairPose frame = sampleAnimation(current_, 0U, animationVariation_);
    frame.transitionClosure = 1.0F;
    frame.transitioning = true;
    return frame;
  }

  if (phase_ == Phase::BlinkOpening) {
    const std::uint32_t elapsed = nowMs - phaseStartedAt_;
    if (elapsed < kBlinkOpenMs) {
      EyePairPose frame =
          sampleAnimation(current_, elapsed, animationVariation_);
      frame.transitionClosure =
          1.0F - smoothstep(static_cast<float>(elapsed) /
                            static_cast<float>(kBlinkOpenMs));
      frame.transitioning = true;
      return frame;
    }

    phase_ = Phase::Playing;
    return sampleAnimation(current_, nowMs - animationStartedAt_,
                           animationVariation_);
  }

  const std::uint32_t elapsed = nowMs - phaseStartedAt_;
  if (elapsed < kSmoothTransitionMs) {
    const EyePairPose target =
        sampleAnimation(pending_, elapsed, pendingVariation_);
    return interpolatePair(
        transitionFrom_, target,
        smoothstep(static_cast<float>(elapsed) /
                   static_cast<float>(kSmoothTransitionMs)));
  }

  current_ = pending_;
  animationVariation_ = pendingVariation_;
  animationStartedAt_ = phaseStartedAt_;
  phase_ = Phase::Playing;
  return sampleAnimation(current_, elapsed, animationVariation_);
}

AnimationId AnimationEngine::currentAnimation() const { return current_; }

void AnimationEngine::setProfile(const AnimationProfile profile,
                                 const std::uint32_t nowMs) {
  const bool moreAlert = static_cast<std::uint8_t>(profile) >
                         static_cast<std::uint8_t>(profile_);
  profile_ = profile;
  if (moreAlert && started_ && phase_ == Phase::Playing) {
    animationStartedAt_ = nowMs - animationDurationMs(current_);
  }
}

AnimationProfile AnimationEngine::profile() const { return profile_; }

std::uint32_t AnimationEngine::nextRandom() {
  randomState_ ^= randomState_ << 13U;
  randomState_ ^= randomState_ >> 17U;
  randomState_ ^= randomState_ << 5U;
  return randomState_;
}
