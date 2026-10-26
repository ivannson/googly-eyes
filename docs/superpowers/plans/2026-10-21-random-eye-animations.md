# Random Eye Animations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Animate both GC9A01 eyes with all approved expressions and coherent random transitions.

**Architecture:** A platform-independent `AnimationEngine` produces a pair of eye poses from elapsed time and a seeded PRNG. The Arduino entry point renders those poses to the two displays at a fixed cadence; random personality mode is the scheduler around the 13 concrete clips.

**Tech Stack:** C++17, Arduino ESP32, PlatformIO, Adafruit GFX, Adafruit GC9A01A

**Spec:** `docs/superpowers/specs/2026-10-21-random-eye-animations-design.md`

## Global Constraints

- Preserve the existing ESP32-C3 pin mapping.
- Use 13 concrete animation clips plus the always-running random personality scheduler.
- Avoid immediate animation repeats.
- Use blink transitions except next to explicit blink animations, where eased pose transitions are used.
- Keep the animation loop non-blocking and target 10 FPS at 40 MHz SPI.

## Review Focus

- A random choice that maps to the current clip must still choose a different valid clip; test every selector bucket.
- Millisecond counter rollover must not stall animation timing; test elapsed arithmetic across `UINT32_MAX`.
- Blink transitions must switch clips only while fully closed; test the closing midpoint and opening start.
- Pupil centers and radii must stay within the visible eye area; test every clip across representative timestamps.
- Explicit blink clips must not receive an additional transition blink; test all three blink IDs on both sides of a boundary.

---

### Task 1: Platform-independent animation engine

**Files:**
- Create: `include/animation_engine.h`
- Create: `src/animation_engine.cpp`
- Create: `test/test_animation_engine.cpp`

**Interfaces:**
- Produces: `AnimationId`, `EyePose`, `EyePairPose`, and `AnimationEngine::update(uint32_t nowMs)`.
- Produces: `chooseNextAnimation(AnimationId current, uint32_t randomValue)` and `usesBlinkTransition(AnimationId current, AnimationId next)` for deterministic tests.

- [x] Write failing tests for valid non-repeating selection, transition policy, rollover-safe elapsed timing, full-closure switching, and bounded poses for every clip.
- [x] Run the host test and confirm it fails because the animation engine does not exist.
- [x] Implement the seeded PRNG, scheduler, transition phases, easing, and all 13 clip samplers.
- [x] Run the host test with `-Wall -Wextra -Werror` and confirm it passes.

### Task 2: Dual-display renderer and main loop

**Files:**
- Modify: `src/main.cpp`
- Modify: `platformio.ini`

**Interfaces:**
- Consumes: `AnimationEngine::update(uint32_t)` and renders its `EyePairPose`.

- [x] Replace the static eye drawing with a renderer for pupil size, pupil position, eyelid closure, and eyelid slant.
- [x] Drive the renderer from a non-blocking 10 FPS loop and seed the engine with `esp_random()`.
- [x] Raise display SPI frequency to 40 MHz and build the ESP32-C3 firmware.

### Task 3: Documentation and hardware verification

**Files:**
- Modify: `README.md`

**Interfaces:**
- Documents the automatic behavior and build/upload workflow.

- [x] Document the animation list, random scheduler, and blink-transition policy.
- [x] Run all host test binaries and a fresh PlatformIO build.
- [x] Upload to `/dev/cu.usbmodem101` and verify the flash hashes.
