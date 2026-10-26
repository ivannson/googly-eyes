# LD2410 Presence-Driven Eye Behavior Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Correct both physically rotated displays and make the eyes sleep, wake, become wary, hide, peek, and reappear in response to LD2410 presence and distance trends.

**Architecture:** Keep the existing animation and framebuffer pipeline, adding a host-testable behavior supervisor above it. A small hardware adapter converts LD2410 readings into neutral observations; the supervisor filters distance, selects an animation profile, and returns an eyelid/pupil overlay. `main.cpp` remains responsible only for hardware polling, display transfer, and composing those outputs.

**Tech Stack:** PlatformIO, Arduino ESP32-C3, Adafruit GC9A01A/GFX, `ncmreynolds/ld2410@^0.2.2`, C++17 host tests using `assert`.

**Spec:** `docs/superpowers/specs/2026-10-23-ld2410-presence-behavior-design.md`

## Global Constraints

- Preserve the existing display pins, shared 27 MHz SPI bus, synchronized broadcast optimization, 60 ms frame schedule, and native USB debug serial.
- LD2410 UART is `Serial1` at 256000 baud, 8-N-1: ESP32 GPIO20 receives radar TX and GPIO21 transmits to radar RX.
- The optional radar OUT pin is not required.
- Left physical rotation is 90 degrees counter-clockwise; right physical rotation is 90 degrees clockwise.
- Absence delay is 3000 ms; wake requires two consecutive present observations.
- Far/normal boundary is 300 cm; hide entry is 150 cm; hide exit is 170 cm; close re-entry during retreat is 135 cm; normal re-entry is 320 cm.
- Approach or retreat requires at least 25 cm filtered change over approximately 1000 ms.
- Hidden peek delay varies from 3000–5000 ms and each peek lasts about 700 ms.
- Sensor disconnection or stale observations lead to Sleeping.
- Keep all behavior decisions free of Arduino and display-library dependencies.
- This workspace is not a Git repository; replace commit steps with test-backed workspace checkpoints and do not initialize Git.

## Review Focus

- A connected radar frame with presence but both target distances zero must not be interpreted as a person at 0 cm or trigger hiding; Task 3 tests this.
- When moving and stationary targets are both reported, the nearer non-zero target must drive behavior; Task 3 tests this.
- Distance jitter smaller than 25 cm around 150 cm and 300 cm must not chatter between states; Task 4 tests this.
- Stale/disconnected radar data and `millis()` rollover must still converge to Sleeping with correct timers; Task 4 tests this.
- Long display transfers must not starve UART parsing; Task 5 calls `radar.read()` before the frame-due early return and verifies that placement by code review and runtime logging.

---

### Task 1: Display orientation correction

**Files:**
- Create: `include/display_orientation.h`
- Create: `test/test_display_orientation.cpp`
- Create: `.superpowers/sdd/2026-10-23-ld2410-presence-behavior/progress.md`
- Modify: `src/main.cpp`

**Interfaces:**
- Produces: `enum class EyeDisplay : std::uint8_t { Left, Right };`
- Produces: `constexpr std::uint8_t displayRotation(EyeDisplay display);`
- Left returns Adafruit rotation `1` (software clockwise); right returns rotation `3` (software counter-clockwise).

- [ ] **Step 1: Write the failing orientation test**

Create `test/test_display_orientation.cpp` with `physicalMountingIsCompensated()` asserting:

```cpp
assert(displayRotation(EyeDisplay::Left) == 1U);
assert(displayRotation(EyeDisplay::Right) == 3U);
```

- [ ] **Step 2: Run the test and verify RED**

Run:

```sh
c++ -std=c++17 -Wall -Wextra -pedantic -Iinclude test/test_display_orientation.cpp -o /tmp/googly-eyes-test-orientation
```

Expected: compilation fails because `display_orientation.h` does not exist.

- [ ] **Step 3: Implement the mapping**

Create the header-only `EyeDisplay` enum and `displayRotation(EyeDisplay)` function. In `setup()`, immediately after both `begin()` calls, invoke:

```cpp
leftEye.setRotation(displayRotation(EyeDisplay::Left));
rightEye.setRotation(displayRotation(EyeDisplay::Right));
```

- [ ] **Step 4: Verify GREEN and firmware compilation**

Run the orientation test binary, then `pio run`. Expected: both exit 0.

- [ ] **Step 5: Record a workspace checkpoint**

Confirm Task 1 files and successful commands in `.superpowers/sdd/2026-10-23-ld2410-presence-behavior/progress.md`.

---

### Task 2: Distance-sensitive animation profiles

**Files:**
- Modify: `include/animation_engine.h`
- Modify: `src/animation_engine.cpp`
- Modify: `test/test_animation_engine.cpp`

**Interfaces:**
- Produces: `enum class AnimationProfile : std::uint8_t { Normal, Cautious, Approaching };`
- Changes: `chooseNextAnimation(AnimationId current, std::uint32_t randomValue, AnimationProfile profile = AnimationProfile::Normal)`.
- Produces: `void AnimationEngine::setProfile(AnimationProfile profile, std::uint32_t nowMs);`
- Produces: `AnimationProfile AnimationEngine::profile() const;`
- `Cautious` increases `SuspiciousSquint` relative to Normal.
- `Approaching` makes `SuspiciousSquint` and `AngryGlare` the dominant combined weight and gives `DizzyOrbit` and `CrazyEyes` zero weight.
- Raising alertness requests an early transition through the existing blink/smooth transition machinery; lowering alertness waits for the current clip to finish.

- [ ] **Step 1: Write failing profile-selection tests**

Add tests named:

- `cautiousProfileSelectsMoreSquintsThanNormal()` counting 10,600 deterministic tickets.
- `approachingProfileDominatesWithSquintAndAnger()` asserting their combined count exceeds half of selections.
- `approachingProfileExcludesDizzyAndCrazy()` asserting both counts are zero.
- `increasingAlertnessRequestsAnEarlyTransition()` starting IdleWander, switching to Approaching at 1000 ms, and asserting a transition begins before the 6500 ms idle duration.
- `loweringAlertnessDoesNotInterruptCurrentAnimation()` asserting the current clip remains playing.

- [ ] **Step 2: Run the animation test and verify RED**

Run:

```sh
c++ -std=c++17 -Wall -Wextra -pedantic -Iinclude test/test_animation_engine.cpp src/animation_engine.cpp -o /tmp/googly-eyes-test-animation && /tmp/googly-eyes-test-animation
```

Expected: compilation fails because `AnimationProfile` and `setProfile` do not exist.

- [ ] **Step 3: Implement weighted profiles**

Add three fixed weight tables and a helper selecting the correct table. Track `profile_` in `AnimationEngine`. `setProfile()` compares profile alertness ranks; when rank increases during `Playing`, move `animationStartedAt_` back by the active clip duration so the next `update(nowMs)` enters the existing transition path.

- [ ] **Step 4: Run all animation tests and verify GREEN**

Run the Task 2 test command. Expected: exit 0 with no warnings or assertion failures.

- [ ] **Step 5: Record a workspace checkpoint**

Update the progress ledger with the test result and changed interfaces.

---

### Task 3: Radar observation normalization

**Files:**
- Create: `include/radar_observation.h`
- Create: `src/radar_observation.cpp`
- Create: `test/test_radar_observation.cpp`

**Interfaces:**
- Produces:

```cpp
struct RadarObservation {
  bool connected;
  bool presence;
  bool movingTarget;
  std::uint16_t distanceCm;
  std::uint32_t observedAtMs;
};

std::uint16_t nearestTargetDistanceCm(
    bool movingDetected, std::uint16_t movingDistanceCm,
    bool stationaryDetected, std::uint16_t stationaryDistanceCm);
```

- A zero distance is invalid. If both valid target distances exist, return the smaller. If neither is valid, return zero for unknown.

- [ ] **Step 1: Write failing normalization tests**

Create tests named:

- `nearerMovingTargetWins()` with 220 cm moving and 410 cm stationary, expecting 220.
- `nearerStationaryTargetWins()` with 450 cm moving and 180 cm stationary, expecting 180.
- `zeroDistancesAreIgnored()` expecting the non-zero target.
- `noValidDistanceReturnsUnknown()` expecting zero rather than a close target.

- [ ] **Step 2: Run the test and verify RED**

Run:

```sh
c++ -std=c++17 -Wall -Wextra -pedantic -Iinclude test/test_radar_observation.cpp src/radar_observation.cpp -o /tmp/googly-eyes-test-radar
```

Expected: compilation fails because the files/interfaces do not exist.

- [ ] **Step 3: Implement observation normalization**

Implement only the pure data type and nearest-valid-distance selection. Keep the LD2410 library out of this file.

- [ ] **Step 4: Verify GREEN**

Build and run the Task 3 test binary. Expected: exit 0.

- [ ] **Step 5: Record a workspace checkpoint**

Update the progress ledger with Task 3 results.

---

### Task 4: Presence behavior state machine and eyelid overlay

**Files:**
- Create: `include/presence_behavior.h`
- Create: `src/presence_behavior.cpp`
- Create: `test/test_presence_behavior.cpp`

**Interfaces:**
- Consumes: `RadarObservation`, `AnimationProfile`, and `EyePairPose`.
- Produces:

```cpp
enum class PresenceState : std::uint8_t {
  Sleeping, Waking, Normal, Cautious, Approaching,
  Hidden, Peeking, Retreating
};

struct PresenceOutput {
  PresenceState state;
  AnimationProfile profile;
  float eyelidClosure;
  float pupilXOffset;
  bool becameVisible;
};

class PresenceBehavior {
 public:
  explicit PresenceBehavior(std::uint32_t seed);
  PresenceOutput update(const RadarObservation &observation,
                        std::uint32_t nowMs);
};

void applyPresenceOverlay(EyePairPose &pose, const PresenceOutput &output);
```

- Distance filtering uses a short exponential moving average and a roughly 1000 ms comparison sample.
- Sleep closure is `1.0`; wake eases from `1.0` to `0.0` over 700 ms; hide eases closed over 350 ms.
- Peek closure is approximately `0.72`, lasts 700 ms, and includes a small bounded pupil X offset.
- `becameVisible` is true when entering Waking or Retreating so integration can reset the random engine to a clean idle pose.

- [ ] **Step 1: Write failing state-machine tests**

Add focused tests for:

- `absenceForThreeSecondsSleeps()` and `shortAbsenceDoesNotSleep()`.
- `twoPresentFramesWakeAndOpenOverSevenHundredMilliseconds()`.
- `farPresenceIsNormal()` at 350 cm.
- `stableNearPresenceIsCautious()` at 220 cm.
- `decreasingDistanceBecomesApproaching()` using samples separated by about 1000 ms and at least 25 cm.
- `subThresholdNoiseDoesNotBecomeApproaching()` using changes below 25 cm.
- `confirmedDistanceBelowOnePointFiveMetresHides()`.
- `hiddenEyesPeekBetweenThreeAndFiveSeconds()` and `peekLastsSevenHundredMilliseconds()`.
- `retreatTrendLeavesHiding()` and `onePointSevenMetresLeavesHiding()`.
- `retreatHysteresisDoesNotImmediatelyRehide()` above 135 cm.
- `staleOrDisconnectedSensorEventuallySleeps()`.
- `zeroUnknownDistanceDoesNotHide()`.
- `timersSurviveMillisRollover()`.
- `overlayClosesBothLidsWithoutBlankingPose()` asserting pupils remain defined while closure reaches 1.0.

- [ ] **Step 2: Run the behavior test and verify RED**

Run:

```sh
c++ -std=c++17 -Wall -Wextra -pedantic -Iinclude test/test_presence_behavior.cpp src/presence_behavior.cpp src/animation_engine.cpp -o /tmp/googly-eyes-test-presence
```

Expected: compilation fails because the behavior interfaces do not exist.

- [ ] **Step 3: Implement filtering and state transitions**

Use unsigned subtraction for all elapsed-time checks. Store confirmation counters, filtered/current/history distances, state-entry time, last valid observation time, closest hidden distance, and PRNG state. Apply the exact thresholds and timing from Global Constraints.

- [ ] **Step 4: Implement the pose overlay**

Set `pose.transitionClosure` to the maximum of its existing value and `eyelidClosure`. Apply `pupilXOffset` equally and clamp both pupils within drawable bounds. Do not replace the framebuffer or power state.

- [ ] **Step 5: Run behavior and existing host suites**

Run the Task 4 binary followed by all existing host-test build/run commands. Expected: every binary exits 0 with no warnings.

- [ ] **Step 6: Record a workspace checkpoint**

Update the progress ledger with state-machine coverage and suite results.

---

### Task 5: LD2410 and main-loop integration

**Files:**
- Modify: `platformio.ini`
- Modify: `src/main.cpp`
- Modify: `README.md`
- Modify: `.superpowers/sdd/2026-10-23-ld2410-presence-behavior/progress.md`

**Interfaces:**
- Consumes all earlier task interfaces.
- Adds PlatformIO dependency `ncmreynolds/ld2410@^0.2.2`.
- Uses `ld2410 radar;`, `Serial1.begin(256000, SERIAL_8N1, 20, 21)`, and `radar.begin(Serial1)`.

- [ ] **Step 1: Add the library and initialize hardware**

Add the pinned dependency, UART pin constants, radar object, and behavior controller. Initialize the display rotations from Task 1 and the UART/radar after native USB serial setup. Log radar connection success or failure without blocking forever.

- [ ] **Step 2: Adapt live readings into `RadarObservation`**

On every `loop()` iteration, call `radar.read()` before any frame-timing return. Build the observation using `presenceDetected()`, moving/stationary detection and distance methods, and `nearestTargetDistanceCm()`.

- [ ] **Step 3: Apply behavior to each frame**

Update the behavior controller, pass its profile to the animation engine, reset the engine to a fresh idle seed on `becameVisible`, then apply the returned overlay before `drawFrame()`. Keep the existing 60 ms schedule and dual-display transfer logic.

- [ ] **Step 4: Add diagnostics and documentation**

Once per state change, log connection, state name, filtered/selected distance, and direction classification. Update README with the full LD2410 wiring, rotation compensation, state thresholds, behavior summary, and 5V power warning.

- [ ] **Step 5: Run fresh full verification**

Run every host test, then:

```sh
pio run
pio run --target upload
pio device monitor --port /dev/cu.usbmodem101 --baud 115200
```

Expected: tests and build exit 0; upload hashes verify; runtime reports radar connection and state transitions without resets or serial parser errors.

- [ ] **Step 6: Perform the physical behavior check**

Walk through no-presence, farther than 3 m, approach through 3 m, cross 1.5 m, wait for a peek, and move away. Record observed thresholds and any final tuning required; do not claim the physical behavior is verified without this user-observed check.

- [ ] **Step 7: Final workspace checkpoint**

Record exact test/build/upload/runtime evidence and any physical-check limitation in the progress ledger.
