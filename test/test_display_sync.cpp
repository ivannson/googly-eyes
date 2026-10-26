#include <cassert>

#include "display_sync.h"

namespace {
constexpr EyePose kNeutral = {
    120.0F, 120.0F, 45.0F, 0.0F, 0.0F, 0.0F,
};

void identicalEyesCanUseOneSharedTransfer() {
  const EyePairPose frame = {
      kNeutral, kNeutral, AnimationId::IdleWander, 0.0F, false,
  };

  assert(canBroadcastEyeFrame(frame));
}

void differentPupilsRequireIndependentTransfers() {
  EyePairPose frame = {
      kNeutral, kNeutral, AnimationId::CrossEyed, 0.0F, false,
  };
  frame.left.pupilX = 150.0F;
  frame.right.pupilX = 90.0F;

  assert(!canBroadcastEyeFrame(frame));
}

void differentLidsRequireIndependentTransfers() {
  EyePairPose frame = {
      kNeutral, kNeutral, AnimationId::IndependentBlink, 0.0F, false,
  };
  frame.left.upperLidClosure = 1.0F;

  assert(!canBroadcastEyeFrame(frame));
}
}  // namespace

int main() {
  identicalEyesCanUseOneSharedTransfer();
  differentPupilsRequireIndependentTransfers();
  differentLidsRequireIndependentTransfers();
  return 0;
}
