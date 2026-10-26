#pragma once

#include <cstdint>

enum class EyeDisplay : std::uint8_t { Left, Right };

constexpr std::uint8_t displayRotation(const EyeDisplay display) {
  return display == EyeDisplay::Left ? 1U : 3U;
}
