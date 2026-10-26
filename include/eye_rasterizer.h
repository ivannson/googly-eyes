#pragma once

#include <cstddef>
#include <cstdint>

#include "animation_engine.h"

constexpr std::uint16_t kEyeWidth = 240U;
constexpr std::uint16_t kEyeHeight = 240U;
constexpr std::size_t kEyePixelCount =
    static_cast<std::size_t>(kEyeWidth) * kEyeHeight;
constexpr std::uint16_t kEyeBlack = 0x0000U;
constexpr std::uint16_t kEyeWhite = 0xFFFFU;

void composeEyeFrame(const EyePose &eye, float transitionClosure,
                     std::uint16_t *pixels);
