#pragma once

#ifndef GOOGLY_EYES_ENABLE_MMWAVE
#define GOOGLY_EYES_ENABLE_MMWAVE 0
#endif

struct EyeControlPolicy {
  bool initializeRadar;
  bool pollRadar;
  bool applyPresenceBehavior;
};

constexpr EyeControlPolicy eyeControlPolicy() {
  return EyeControlPolicy{GOOGLY_EYES_ENABLE_MMWAVE != 0,
                          GOOGLY_EYES_ENABLE_MMWAVE != 0,
                          GOOGLY_EYES_ENABLE_MMWAVE != 0};
}
