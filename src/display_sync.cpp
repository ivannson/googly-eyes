#include "display_sync.h"

namespace {
bool sameEyePose(const EyePose &left, const EyePose &right) {
  return left.pupilX == right.pupilX && left.pupilY == right.pupilY &&
         left.pupilRadius == right.pupilRadius &&
         left.upperLidClosure == right.upperLidClosure &&
         left.lowerLidClosure == right.lowerLidClosure &&
         left.upperLidSlant == right.upperLidSlant;
}
}  // namespace

bool canBroadcastEyeFrame(const EyePairPose &frame) {
  return sameEyePose(frame.left, frame.right);
}
