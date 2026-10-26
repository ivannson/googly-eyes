#pragma once

#include <cstdint>

constexpr std::uint32_t kEyeFrameIntervalMs = 60U;

constexpr bool isEyeFrameDue(const std::uint32_t nowMs,
                             const std::uint32_t lastFrameAtMs) {
  return nowMs - lastFrameAtMs >= kEyeFrameIntervalMs;
}
