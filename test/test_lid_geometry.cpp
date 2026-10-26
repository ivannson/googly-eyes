#include <cassert>

#include "lid_geometry.h"

namespace {
void openEyeLeavesTheDisplayUncovered() {
  const EyePose neutral = {120.0F, 120.0F, 45.0F, 0.0F, 0.0F, 0.0F};
  const LidGeometry lids = makeLidGeometry(neutral, 0.0F);
  assert(lids.upperLeftY == 0);
  assert(lids.upperRightY == 0);
  assert(lids.lowerY == 240);
}

void transitionBlinkFullyCoversTheEye() {
  const EyePose neutral = {120.0F, 120.0F, 45.0F, 0.0F, 0.0F, 0.0F};
  const LidGeometry lids = makeLidGeometry(neutral, 1.0F);
  assert(lids.upperLeftY == 120);
  assert(lids.upperRightY == 120);
  assert(lids.lowerY == 120);
}

void positiveSlantLowersTheRightSideOfTheUpperLid() {
  const EyePose angry = {120.0F, 120.0F, 45.0F, 0.3F, 0.0F, 30.0F};
  const LidGeometry lids = makeLidGeometry(angry, 0.0F);
  assert(lids.upperLeftY == 21);
  assert(lids.upperRightY == 51);
}

void fullTransitionClosureFlattensAnExpressionSlant() {
  const EyePose angry = {120.0F, 120.0F, 45.0F, 0.3F, 0.0F, 30.0F};
  const LidGeometry lids = makeLidGeometry(angry, 1.0F);
  assert(lids.upperLeftY == 120);
  assert(lids.upperRightY == 120);
  assert(lids.lowerY == 120);
}

void squintAndAngerLeaveANarrowHalfAreaOpening() {
  const AnimationId expressions[] = {
      AnimationId::SuspiciousSquint,
      AnimationId::AngryGlare,
  };

  for (const AnimationId expression : expressions) {
    const EyePairPose frame = sampleAnimation(expression, 2000U, 1U);
    const EyePose eyes[] = {frame.left, frame.right};
    for (const EyePose &eye : eyes) {
      const LidGeometry lids = makeLidGeometry(eye, 0.0F);
      const int upperCenter = (lids.upperLeftY + lids.upperRightY) / 2;
      const int openingHeight = lids.lowerY - upperCenter;
      assert(openingHeight >= 90);
      assert(openingHeight <= 105);
    }
  }
}
}  // namespace

int main() {
  openEyeLeavesTheDisplayUncovered();
  transitionBlinkFullyCoversTheEye();
  positiveSlantLowersTheRightSideOfTheUpperLid();
  fullTransitionClosureFlattensAnExpressionSlant();
  squintAndAngerLeaveANarrowHalfAreaOpening();
  return 0;
}
