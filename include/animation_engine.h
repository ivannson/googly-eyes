#pragma once

#include <cstdint>

enum class AnimationId : std::uint8_t {
  IdleWander = 0,
  QuickGlance,
  SingleBlink,
  DoubleBlink,
  IndependentBlink,
  SuspiciousSquint,
  AngryGlare,
  Surprised,
  CrossEyed,
  DizzyOrbit,
  SleepWake,
  CrazyEyes,
  TrackingSweep,
  Count,
};

enum class AnimationProfile : std::uint8_t {
  Normal = 0,
  Cautious,
  Approaching,
};

constexpr std::uint8_t kAnimationCount =
    static_cast<std::uint8_t>(AnimationId::Count);

struct EyePose {
  float pupilX;
  float pupilY;
  float pupilRadius;
  float upperLidClosure;
  float lowerLidClosure;
  float upperLidSlant;
};

struct EyePairPose {
  EyePose left;
  EyePose right;
  AnimationId animation;
  float transitionClosure;
  bool transitioning;
};

bool isBlinkAnimation(AnimationId animation);
AnimationId chooseNextAnimation(
    AnimationId current, std::uint32_t randomValue,
    AnimationProfile profile = AnimationProfile::Normal);
bool usesBlinkTransition(AnimationId current, AnimationId next);
std::uint32_t animationDurationMs(AnimationId animation);
EyePairPose sampleAnimation(AnimationId animation, std::uint32_t elapsedMs,
                            std::uint32_t variation);

class AnimationEngine {
 public:
  explicit AnimationEngine(std::uint32_t seed);

  EyePairPose update(std::uint32_t nowMs);
  void setProfile(AnimationProfile profile, std::uint32_t nowMs);
  AnimationProfile profile() const;
  AnimationId currentAnimation() const;

 private:
  enum class Phase : std::uint8_t {
    Playing,
    BlinkClosing,
    BlinkOpening,
    SmoothTransition,
  };

  std::uint32_t nextRandom();

  std::uint32_t randomState_;
  std::uint32_t phaseStartedAt_;
  std::uint32_t animationStartedAt_;
  std::uint32_t animationVariation_;
  std::uint32_t pendingVariation_;
  AnimationId current_;
  AnimationId pending_;
  Phase phase_;
  AnimationProfile profile_;
  bool started_;
  EyePairPose transitionFrom_;
};
