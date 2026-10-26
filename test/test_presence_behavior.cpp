#include <cassert>
#include <cmath>
#include <cstdint>
#include <limits>

#include "presence_behavior.h"

namespace {
RadarObservation observation(const bool connected, const bool presence,
                             const std::uint16_t distance,
                             const std::uint32_t now,
                             const bool moving = true) {
  return {connected, presence, moving, distance, now};
}

PresenceOutput wakeAt(PresenceBehavior &behavior, const std::uint16_t distance,
                      const std::uint32_t start = 0U) {
  behavior.update(observation(true, true, distance, start), start);
  const PresenceOutput waking = behavior.update(
      observation(true, true, distance, start + 60U), start + 60U);
  assert(waking.state == PresenceState::Waking);
  assert(waking.becameVisible);
  return behavior.update(observation(true, true, distance, start + 800U),
                         start + 800U);
}

void absenceForThreeSecondsSleeps() {
  PresenceBehavior behavior(1U);
  assert(wakeAt(behavior, 350U).state == PresenceState::Normal);
  assert(behavior.update(observation(true, false, 0U, 1000U), 1000U).state !=
         PresenceState::Sleeping);
  const PresenceOutput asleep =
      behavior.update(observation(true, false, 0U, 4000U), 4000U);
  assert(asleep.state == PresenceState::Sleeping);
  assert(asleep.eyelidClosure == 1.0F);
}

void shortAbsenceDoesNotSleep() {
  PresenceBehavior behavior(2U);
  wakeAt(behavior, 350U);
  behavior.update(observation(true, false, 0U, 1000U), 1000U);
  assert(behavior.update(observation(true, false, 0U, 3999U), 3999U).state !=
         PresenceState::Sleeping);
  assert(behavior.update(observation(true, true, 350U, 4050U), 4050U).state !=
         PresenceState::Sleeping);
}

void twoPresentFramesWakeAndOpenOverSevenHundredMilliseconds() {
  PresenceBehavior behavior(3U);
  assert(behavior.update(observation(true, true, 350U, 0U), 0U).state ==
         PresenceState::Sleeping);
  const PresenceOutput waking =
      behavior.update(observation(true, true, 350U, 60U), 60U);
  assert(waking.state == PresenceState::Waking);
  assert(waking.eyelidClosure > 0.99F);
  const PresenceOutput halfway =
      behavior.update(observation(true, true, 350U, 410U), 410U);
  assert(halfway.eyelidClosure > 0.40F);
  assert(halfway.eyelidClosure < 0.60F);
  const PresenceOutput open =
      behavior.update(observation(true, true, 350U, 760U), 760U);
  assert(open.state == PresenceState::Normal);
  assert(open.eyelidClosure == 0.0F);
}

void sameRadarFrameCannotWakeTwice() {
  PresenceBehavior behavior(31U);
  behavior.update(observation(true, true, 350U, 100U), 100U);
  const PresenceOutput cached =
      behavior.update(observation(true, true, 350U, 100U), 160U);
  assert(cached.state == PresenceState::Sleeping);
  const PresenceOutput fresh =
      behavior.update(observation(true, true, 350U, 220U), 220U);
  assert(fresh.state == PresenceState::Waking);
}

void farPresenceIsNormal() {
  PresenceBehavior behavior(4U);
  assert(wakeAt(behavior, 350U).state == PresenceState::Normal);
}

void stableNearPresenceIsCautious() {
  PresenceBehavior behavior(5U);
  const PresenceOutput output = wakeAt(behavior, 220U);
  assert(output.state == PresenceState::Cautious);
  assert(output.profile == AnimationProfile::Cautious);
}

void decreasingDistanceBecomesApproaching() {
  PresenceBehavior behavior(6U);
  wakeAt(behavior, 280U);
  const PresenceOutput output =
      behavior.update(observation(true, true, 180U, 2000U), 2000U);
  assert(output.state == PresenceState::Approaching);
  assert(output.profile == AnimationProfile::Approaching);
}

void subThresholdNoiseDoesNotBecomeApproaching() {
  PresenceBehavior behavior(7U);
  wakeAt(behavior, 240U);
  const PresenceOutput output =
      behavior.update(observation(true, true, 220U, 2000U), 2000U);
  assert(output.state != PresenceState::Approaching);
}

void confirmedDistanceBelowOnePointFiveMetresHides() {
  PresenceBehavior behavior(8U);
  wakeAt(behavior, 220U);
  behavior.update(observation(true, true, 80U, 1000U), 1000U);
  const PresenceOutput hidden =
      behavior.update(observation(true, true, 80U, 1060U), 1060U);
  assert(hidden.state == PresenceState::Hidden);
}

void alternatingBoundarySamplesDoNotChatter() {
  PresenceBehavior behavior(9U);
  wakeAt(behavior, 350U);
  behavior.update(observation(true, true, 295U, 1000U), 1000U);
  behavior.update(observation(true, true, 295U, 1060U), 1060U);
  behavior.update(observation(true, true, 295U, 1120U), 1120U);
  const PresenceOutput cautious =
      behavior.update(observation(true, true, 305U, 1180U), 1180U);
  assert(cautious.state != PresenceState::Normal);

  behavior.update(observation(true, true, 146U, 1240U), 1240U);
  const PresenceOutput nearBoundary =
      behavior.update(observation(true, true, 154U, 1300U), 1300U);
  assert(nearBoundary.state != PresenceState::Hidden);
}

void enterHidden(PresenceBehavior &behavior) {
  wakeAt(behavior, 220U);
  behavior.update(observation(true, true, 60U, 1000U), 1000U);
  const PresenceOutput hidden =
      behavior.update(observation(true, true, 60U, 1060U), 1060U);
  assert(hidden.state == PresenceState::Hidden);
}

void hiddenEyesPeekBetweenThreeAndFiveSeconds() {
  PresenceBehavior behavior(10U);
  enterHidden(behavior);
  assert(behavior.update(observation(true, true, 60U, 4059U), 4059U).state ==
         PresenceState::Hidden);
  const PresenceOutput peek =
      behavior.update(observation(true, true, 60U, 6061U), 6061U);
  assert(peek.state == PresenceState::Peeking);
  assert(peek.eyelidClosure > 0.65F && peek.eyelidClosure < 0.80F);
  assert(std::fabs(peek.pupilXOffset) > 1.0F);
}

void peekLastsSevenHundredMilliseconds() {
  PresenceBehavior behavior(11U);
  enterHidden(behavior);
  behavior.update(observation(true, true, 60U, 6061U), 6061U);
  assert(behavior.update(observation(true, true, 60U, 6760U), 6760U).state ==
         PresenceState::Peeking);
  assert(behavior.update(observation(true, true, 60U, 6761U), 6761U).state ==
         PresenceState::Hidden);
}

void retreatTrendLeavesHiding() {
  PresenceBehavior behavior(12U);
  enterHidden(behavior);
  const PresenceOutput retreating =
      behavior.update(observation(true, true, 190U, 2200U), 2200U);
  assert(retreating.state == PresenceState::Retreating);
  assert(retreating.becameVisible);
}

void onePointSevenMetresLeavesHiding() {
  PresenceBehavior behavior(13U);
  enterHidden(behavior);
  behavior.update(observation(true, true, 300U, 1120U), 1120U);
  const PresenceOutput retreating =
      behavior.update(observation(true, true, 300U, 1180U), 1180U);
  assert(retreating.state == PresenceState::Retreating);
}

void retreatHysteresisDoesNotImmediatelyRehide() {
  PresenceBehavior behavior(14U);
  enterHidden(behavior);
  behavior.update(observation(true, true, 260U, 1120U), 1120U);
  assert(behavior.update(observation(true, true, 260U, 1180U), 1180U).state ==
         PresenceState::Retreating);
  assert(behavior.update(observation(true, true, 145U, 1240U), 1240U).state ==
         PresenceState::Retreating);
}

void staleOrDisconnectedSensorEventuallySleeps() {
  PresenceBehavior disconnected(15U);
  wakeAt(disconnected, 350U);
  disconnected.update(observation(false, false, 0U, 1000U), 1000U);
  assert(disconnected.update(observation(false, false, 0U, 4000U), 4000U)
             .state == PresenceState::Sleeping);

  PresenceBehavior stale(16U);
  wakeAt(stale, 350U);
  stale.update(observation(true, true, 350U, 800U), 1000U);
  assert(stale.update(observation(true, true, 350U, 800U), 5500U).state ==
         PresenceState::Sleeping);
}

void zeroUnknownDistanceDoesNotHide() {
  PresenceBehavior behavior(17U);
  wakeAt(behavior, 220U);
  for (std::uint32_t now = 1000U; now < 1400U; now += 60U) {
    const PresenceOutput output =
        behavior.update(observation(true, true, 0U, now), now);
    assert(output.state != PresenceState::Hidden);
  }
}

void unknownDistanceClearsPendingHideConfirmation() {
  PresenceBehavior behavior(32U);
  wakeAt(behavior, 220U);
  behavior.update(observation(true, true, 80U, 1000U), 1000U);
  const PresenceOutput unknown =
      behavior.update(observation(true, true, 0U, 1060U), 1060U);
  assert(unknown.state != PresenceState::Hidden);
  const PresenceOutput nextClose =
      behavior.update(observation(true, true, 80U, 1120U), 1120U);
  assert(nextClose.state != PresenceState::Hidden);
}

void sleepExpiresHistoricalCloseDistance() {
  PresenceBehavior behavior(33U);
  wakeAt(behavior, 220U);
  behavior.update(observation(true, true, 60U, 1000U), 1000U);
  behavior.update(observation(true, true, 60U, 1060U), 1060U);
  behavior.update(observation(true, false, 0U, 1200U), 1200U);
  assert(behavior.update(observation(true, false, 0U, 4200U), 4200U).state ==
         PresenceState::Sleeping);
  behavior.update(observation(true, true, 0U, 4300U), 4300U);
  const PresenceOutput wakingUnknown =
      behavior.update(observation(true, true, 0U, 4360U), 4360U);
  assert(wakingUnknown.state == PresenceState::Waking);
  assert(behavior.update(observation(true, true, 0U, 5100U), 5100U).state !=
         PresenceState::Hidden);
}

void continuedRetreatBelowRehideDistanceStaysVisible() {
  PresenceBehavior behavior(34U);
  enterHidden(behavior);
  const PresenceOutput retreating =
      behavior.update(observation(true, true, 190U, 2200U), 2200U);
  assert(retreating.state == PresenceState::Retreating);
  assert(behavior.update(observation(true, true, 90U, 2260U), 2260U).state ==
         PresenceState::Retreating);
  assert(behavior.update(observation(true, true, 90U, 2320U), 2320U).state ==
         PresenceState::Retreating);
}

void renewedApproachCanRehideWithoutClosureJump() {
  PresenceBehavior behavior(35U);
  enterHidden(behavior);
  const PresenceOutput retreating =
      behavior.update(observation(true, true, 190U, 2200U), 2200U);
  assert(retreating.state == PresenceState::Retreating);
  behavior.update(observation(true, true, 190U, 2400U), 2400U);

  const PresenceOutput beforeRehide =
      behavior.update(observation(true, true, 70U, 3300U), 3300U);
  const PresenceOutput rehidden =
      behavior.update(observation(true, true, 70U, 3360U), 3360U);
  assert(rehidden.state == PresenceState::Hidden);
  assert(std::fabs(rehidden.eyelidClosure - beforeRehide.eyelidClosure) <
         0.001F);
}

void timersSurviveMillisRollover() {
  PresenceBehavior behavior(18U);
  constexpr std::uint32_t start =
      std::numeric_limits<std::uint32_t>::max() - 1000U;
  wakeAt(behavior, 350U, start);
  behavior.update(observation(true, false, 0U, start + 900U), start + 900U);
  assert(behavior
             .update(observation(true, false, 0U, start + 3900U),
                     start + 3900U)
             .state == PresenceState::Sleeping);
}

void overlayClosesBothLidsWithoutBlankingPose() {
  EyePairPose pose = sampleAnimation(AnimationId::IdleWander, 0U, 1U);
  const PresenceOutput output = {PresenceState::Hidden,
                                 AnimationProfile::Cautious, 1.0F, 12.0F,
                                 false};
  applyPresenceOverlay(pose, output);
  assert(pose.transitionClosure == 1.0F);
  assert(pose.left.pupilRadius > 0.0F);
  assert(pose.right.pupilRadius > 0.0F);
  assert(pose.left.pupilX == 132.0F);
  assert(pose.right.pupilX == 132.0F);
}
}  // namespace

int main() {
  absenceForThreeSecondsSleeps();
  shortAbsenceDoesNotSleep();
  twoPresentFramesWakeAndOpenOverSevenHundredMilliseconds();
  sameRadarFrameCannotWakeTwice();
  farPresenceIsNormal();
  stableNearPresenceIsCautious();
  decreasingDistanceBecomesApproaching();
  subThresholdNoiseDoesNotBecomeApproaching();
  confirmedDistanceBelowOnePointFiveMetresHides();
  alternatingBoundarySamplesDoNotChatter();
  hiddenEyesPeekBetweenThreeAndFiveSeconds();
  peekLastsSevenHundredMilliseconds();
  retreatTrendLeavesHiding();
  onePointSevenMetresLeavesHiding();
  retreatHysteresisDoesNotImmediatelyRehide();
  staleOrDisconnectedSensorEventuallySleeps();
  zeroUnknownDistanceDoesNotHide();
  unknownDistanceClearsPendingHideConfirmation();
  sleepExpiresHistoricalCloseDistance();
  continuedRetreatBelowRehideDistanceStaysVisible();
  renewedApproachCanRehideWithoutClosureJump();
  timersSurviveMillisRollover();
  overlayClosesBothLidsWithoutBlankingPose();
  return 0;
}
