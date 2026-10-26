#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

#include "animation_engine.h"

namespace {
constexpr float kEpsilon = 0.001F;

bool nearlyEqual(const float left, const float right) {
  return std::fabs(left - right) < kEpsilon;
}

void selectionNeverImmediatelyRepeats() {
  for (std::uint8_t current = 0; current < kAnimationCount; ++current) {
    for (std::uint32_t bucket = 0; bucket < 128; ++bucket) {
      const auto id = static_cast<AnimationId>(current);
      const AnimationId next = chooseNextAnimation(id, bucket);
      assert(next != id);
      assert(static_cast<std::uint8_t>(next) < kAnimationCount);
    }
  }
}

void everyOtherAnimationCanBeSelected() {
  for (std::uint8_t current = 0; current < kAnimationCount; ++current) {
    bool seen[kAnimationCount] = {};
    const auto id = static_cast<AnimationId>(current);
    for (std::uint32_t bucket = 0; bucket < 4096U; ++bucket) {
      seen[static_cast<std::uint8_t>(chooseNextAnimation(id, bucket))] = true;
    }
    for (std::uint8_t candidate = 0; candidate < kAnimationCount;
         ++candidate) {
      assert(seen[candidate] == (candidate != current));
    }
  }
}

void calmBehaviorsAreSelectedMoreOftenThanBursts() {
  std::uint32_t counts[kAnimationCount] = {};
  for (std::uint32_t value = 0; value < 10600U; ++value) {
    const AnimationId selected =
        chooseNextAnimation(AnimationId::AngryGlare, value);
    ++counts[static_cast<std::uint8_t>(selected)];
  }

  const std::uint32_t crazy =
      counts[static_cast<std::uint8_t>(AnimationId::CrazyEyes)];
  const std::uint32_t singleBlink =
      counts[static_cast<std::uint8_t>(AnimationId::SingleBlink)];
  const std::uint32_t squint =
      counts[static_cast<std::uint8_t>(AnimationId::SuspiciousSquint)];

  assert(crazy > 0U);
  assert(singleBlink >= crazy * 10U);
  assert(squint >= crazy * 10U);
}

std::uint32_t countSelections(const AnimationId target,
                              const AnimationProfile profile) {
  std::uint32_t count = 0U;
  for (std::uint32_t value = 0U; value < 10600U; ++value) {
    if (chooseNextAnimation(AnimationId::IdleWander, value, profile) ==
        target) {
      ++count;
    }
  }
  return count;
}

void cautiousProfileSelectsMoreSquintsThanNormal() {
  assert(countSelections(AnimationId::SuspiciousSquint,
                         AnimationProfile::Cautious) >
         countSelections(AnimationId::SuspiciousSquint,
                         AnimationProfile::Normal));
}

void approachingProfileDominatesWithSquintAndAnger() {
  const std::uint32_t squints = countSelections(
      AnimationId::SuspiciousSquint, AnimationProfile::Approaching);
  const std::uint32_t anger = countSelections(
      AnimationId::AngryGlare, AnimationProfile::Approaching);
  assert(squints + anger > 5300U);
}

void approachingProfileExcludesDizzyAndCrazy() {
  assert(countSelections(AnimationId::DizzyOrbit,
                         AnimationProfile::Approaching) == 0U);
  assert(countSelections(AnimationId::CrazyEyes,
                         AnimationProfile::Approaching) == 0U);
}

void increasingAlertnessRequestsAnEarlyTransition() {
  AnimationEngine engine(7U);
  engine.update(0U);
  engine.setProfile(AnimationProfile::Approaching, 1000U);
  const EyePairPose frame = engine.update(1001U);
  assert(engine.profile() == AnimationProfile::Approaching);
  assert(frame.transitioning);
}

void loweringAlertnessDoesNotInterruptCurrentAnimation() {
  AnimationEngine engine(7U);
  engine.update(0U);
  engine.setProfile(AnimationProfile::Approaching, 500U);
  engine.update(501U);
  AnimationId animationDuringTransition = engine.currentAnimation();
  engine.setProfile(AnimationProfile::Normal, 600U);
  const EyePairPose frame = engine.update(601U);
  assert(engine.profile() == AnimationProfile::Normal);
  assert(engine.currentAnimation() == animationDuringTransition);
  assert(frame.animation == animationDuringTransition);
}

void calmBehaviorsLastLongerThanCrazyEyes() {
  const std::uint32_t crazy = animationDurationMs(AnimationId::CrazyEyes);
  assert(animationDurationMs(AnimationId::SingleBlink) > crazy);
  assert(animationDurationMs(AnimationId::SuspiciousSquint) > crazy * 2U);
  assert(animationDurationMs(AnimationId::IdleWander) >
         animationDurationMs(AnimationId::QuickGlance));
}

void explicitBlinksDoNotGetTransitionBlinks() {
  const AnimationId blinks[] = {
      AnimationId::SingleBlink,
      AnimationId::DoubleBlink,
      AnimationId::IndependentBlink,
  };
  for (const AnimationId blink : blinks) {
    assert(!usesBlinkTransition(AnimationId::IdleWander, blink));
    assert(!usesBlinkTransition(blink, AnimationId::IdleWander));
  }
  assert(usesBlinkTransition(AnimationId::IdleWander,
                             AnimationId::AngryGlare));
}

void blinkTransitionSwitchesAtFullClosure() {
  // Seed 7 selects SuspiciousSquint after IdleWander, so this exercises
  // the blink-covered transition path rather than an explicit blink clip.
  AnimationEngine engine(7U);
  constexpr std::uint32_t start = 1000U;
  engine.update(start);

  const std::uint32_t transitionStart =
      start + animationDurationMs(AnimationId::IdleWander);
  const EyePairPose starting = engine.update(transitionStart);
  assert(starting.animation == AnimationId::IdleWander);
  assert(starting.transitioning);
  assert(nearlyEqual(starting.transitionClosure, 0.0F));

  const EyePairPose halfClosed = engine.update(transitionStart + 90U);
  assert(halfClosed.animation == AnimationId::IdleWander);
  assert(halfClosed.transitionClosure > 0.45F);
  assert(halfClosed.transitionClosure < 0.55F);

  const EyePairPose switched = engine.update(transitionStart + 180U);
  assert(switched.animation != AnimationId::IdleWander);
  assert(nearlyEqual(switched.transitionClosure, 1.0F));
}

void elapsedTimingSurvivesMillisRollover() {
  AnimationEngine engine(7U);
  constexpr std::uint32_t start =
      std::numeric_limits<std::uint32_t>::max() - 1000U;
  engine.update(start);
  const EyePairPose afterRollover = engine.update(500U);
  assert(afterRollover.animation == AnimationId::IdleWander);
  assert(!afterRollover.transitioning);
}

void everyAnimationKeepsPupilsAndLidsDrawable() {
  for (std::uint8_t rawId = 0; rawId < kAnimationCount; ++rawId) {
    const auto id = static_cast<AnimationId>(rawId);
    const std::uint32_t duration = animationDurationMs(id);
    for (std::uint32_t step = 0; step <= 8; ++step) {
      const EyePairPose frame =
          sampleAnimation(id, duration * step / 8U, 0x12345678U);
      const EyePose eyes[] = {frame.left, frame.right};
      for (const EyePose &eye : eyes) {
        assert(eye.pupilRadius >= 12.0F);
        assert(eye.pupilRadius <= 50.0F);
        assert(eye.pupilX - eye.pupilRadius >= 0.0F);
        assert(eye.pupilX + eye.pupilRadius <= 240.0F);
        assert(eye.pupilY - eye.pupilRadius >= 0.0F);
        assert(eye.pupilY + eye.pupilRadius <= 240.0F);
        assert(eye.upperLidClosure >= 0.0F);
        assert(eye.upperLidClosure <= 1.0F);
        assert(eye.lowerLidClosure >= 0.0F);
        assert(eye.lowerLidClosure <= 1.0F);
        assert(std::fabs(eye.upperLidSlant) <= 60.0F);
      }
    }
  }
}

void namedAnimationsHaveTheirDefiningPose() {
  const EyePairPose single =
      sampleAnimation(AnimationId::SingleBlink, 760U, 1U);
  assert(single.left.upperLidClosure > 0.95F);
  assert(single.right.upperLidClosure > 0.95F);

  const EyePairPose independent =
      sampleAnimation(AnimationId::IndependentBlink, 600U, 1U);
  assert(independent.left.upperLidClosure > 0.5F);
  assert(independent.right.upperLidClosure < 0.1F);

  const EyePairPose suspicious =
      sampleAnimation(AnimationId::SuspiciousSquint, 1200U, 1U);
  assert(suspicious.left.upperLidClosure > 0.2F);
  assert(suspicious.left.pupilX != 120.0F);

  const EyePairPose angry =
      sampleAnimation(AnimationId::AngryGlare, 1200U, 1U);
  assert(angry.left.upperLidSlant > 20.0F);
  assert(angry.right.upperLidSlant < -20.0F);

  const EyePairPose surprised =
      sampleAnimation(AnimationId::Surprised, 1000U, 1U);
  assert(surprised.left.pupilRadius < 25.0F);

  const EyePairPose crossed =
      sampleAnimation(AnimationId::CrossEyed, 1200U, 1U);
  assert(crossed.left.pupilX > 145.0F);
  assert(crossed.right.pupilX < 95.0F);

  const EyePairPose dizzyStart =
      sampleAnimation(AnimationId::DizzyOrbit, 800U, 1U);
  const EyePairPose dizzyLater =
      sampleAnimation(AnimationId::DizzyOrbit, 1100U, 1U);
  assert(!nearlyEqual(dizzyStart.left.pupilX, dizzyLater.left.pupilX));

  const EyePairPose sleeping =
      sampleAnimation(AnimationId::SleepWake, 2200U, 1U);
  assert(sleeping.left.upperLidClosure > 0.95F);
  assert(sleeping.left.lowerLidClosure > 0.95F);

  const EyePairPose crazy =
      sampleAnimation(AnimationId::CrazyEyes, 1000U, 1U);
  assert(!nearlyEqual(crazy.left.pupilX, crazy.right.pupilX));

  const EyePairPose trackingStart =
      sampleAnimation(AnimationId::TrackingSweep, 500U, 1U);
  const EyePairPose trackingLater =
      sampleAnimation(AnimationId::TrackingSweep, 1000U, 1U);
  assert(!nearlyEqual(trackingStart.left.pupilX,
                      trackingLater.left.pupilX));
}
}  // namespace

int main() {
  selectionNeverImmediatelyRepeats();
  everyOtherAnimationCanBeSelected();
  calmBehaviorsAreSelectedMoreOftenThanBursts();
  cautiousProfileSelectsMoreSquintsThanNormal();
  approachingProfileDominatesWithSquintAndAnger();
  approachingProfileExcludesDizzyAndCrazy();
  increasingAlertnessRequestsAnEarlyTransition();
  loweringAlertnessDoesNotInterruptCurrentAnimation();
  calmBehaviorsLastLongerThanCrazyEyes();
  explicitBlinksDoNotGetTransitionBlinks();
  blinkTransitionSwitchesAtFullClosure();
  elapsedTimingSurvivesMillisRollover();
  everyAnimationKeepsPupilsAndLidsDrawable();
  namedAnimationsHaveTheirDefiningPose();
  return 0;
}
