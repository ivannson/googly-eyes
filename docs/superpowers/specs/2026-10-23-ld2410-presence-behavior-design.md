# LD2410 presence-driven eye behavior

Date: 2026-10-23

## Intent

Use the installed Hi-Link LD2410 mmWave sensor to make the eyes react to
presence, distance, and direction of travel while preserving the existing
random personality animations. Correct the rendered orientation for the new
physical mounting: the left display is rotated 90 degrees counter-clockwise
and the right display is rotated 90 degrees clockwise.

The character should feel asleep when alone, relaxed at a distance, wary as a
person approaches, and shy when someone gets too close. Hiding is an eyelid
animation: both screens remain powered and show black closed lids rather than
going blank.

## Hardware and integration

- Keep the existing two-display wiring and 27 MHz shared SPI bus.
- After display initialization, apply a 90-degree clockwise controller
  rotation to the left display and a 90-degree counter-clockwise rotation to
  the right display. The logical 240 x 240 animation coordinate system remains
  unchanged.
- Connect the LD2410 UART TX to ESP32-C3 GPIO20 (RX) and LD2410 UART RX to
  GPIO21 (TX). Power the radar from 5V/VBUS and share ground.
- Run `Serial1` at 256000 baud, 8-N-1. Native USB `Serial` remains the debug
  console.
- Use `ncmreynolds/ld2410` 0.2.x through PlatformIO. Call its non-blocking
  `read()` method on every loop iteration so incoming UART data is drained even
  when no display frame is due. The optional OUT pin is not required.

## Sensor observations

Each valid radar update is converted into a platform-independent observation:

- `connected`
- `presence`
- nearest valid target distance in centimetres
- whether a moving target is present
- observation time

When both moving and stationary targets exist, use the nearer non-zero
distance. Smooth distance with a short exponential filter and compare the
filtered value against its value roughly one second earlier. A decrease of at
least 25 cm is approaching; an increase of at least 25 cm is retreating.
Smaller changes are treated as stationary/noise. Require two consistent radar
frames before wake or close-range transitions and use distance hysteresis so a
target near a boundary does not rapidly switch modes.

If radar data becomes stale or the sensor disconnects, log the condition and
fall back to sleeping rather than inventing a distance.

## Behavior state machine

The state machine is deterministic and uses rollover-safe millisecond timing.
Thresholds are measured from the filtered nearest-target distance.

### Sleeping

- Enter after presence has been absent continuously for 3 seconds.
- Ease both eyelids fully closed, then hold them closed in black.
- Do not run random visible animations while asleep.

### Waking

- Enter after presence is confirmed in two consecutive valid observations.
- Open the eyelids smoothly over about 700 ms.
- Continue into the appropriate distance state after opening.

### Normal

- Used while the person is farther than 3.0 m and is not rapidly approaching.
- Run the existing balanced random animation profile.

### Cautious

- Used from approximately 1.5 m to 3.0 m when distance is stable.
- Retain normal blinks and idle movement but increase suspicious squints.

### Approaching

- Used when the person is moving closer within 3.0 m but remains at or beyond
  the hide threshold.
- Strongly bias new selections toward suspicious squint and angry glare, with
  blinks retained so the result stays alive rather than locked in one face.
- A profile change may request an early blink-covered transition instead of
  waiting through a long calm animation.

### Hidden and peeking

- Enter when confirmed distance is below 1.5 m.
- Close both eyelids over about 350 ms and hold the existing black eyelid
  rendering at full closure; never power down or clear the screens.
- Every 3–5 seconds, briefly part the lids to a narrow slit for about 700 ms.
  During the peek the pupils make a small glance, then the lids close again.
- The peek interval is varied from the existing PRNG so it does not look
  mechanical.

### Retreating

- Leave hiding as soon as a sustained retreat of at least 25 cm over roughly
  one second is detected, or when distance exceeds a 1.7 m exit threshold.
- Open from the hidden pose into a cautious expression. Do not immediately
  re-hide unless the target approaches again below about 1.35 m; this is the
  close-range hysteresis band.
- Return to Normal after distance exceeds 3.2 m. If presence disappears, the
  normal absence delay leads to Sleeping.

## Animation-engine changes

Add explicit selection profiles rather than duplicating the animation engine:

- `Normal`: current balanced weights.
- `Cautious`: higher suspicious-squint weight and lower high-energy novelty
  weights.
- `Approaching`: suspicious squint and angry glare dominate; crazy eyes and
  dizzy orbit are excluded.

The behavior controller selects the profile. Sleep, wake, hide, and peek are
supervisory eyelid overlays applied to the generated `EyePairPose`, keeping the
existing framebuffer, blink transitions, synchronized broadcast optimization,
and frame timing intact.

## Structure

- `radar_observation.h`: hardware-independent input data structure.
- `presence_behavior.h/.cpp`: filtered distance trend, state transitions,
  hysteresis, sleep/wake/hide/peek timing, and pose overlay.
- `animation_engine.h/.cpp`: weighted selection by profile and controlled early
  transition when the behavior profile becomes more alert.
- `main.cpp`: UART/library adapter, display rotations, controller wiring, and
  serial diagnostics.

All decision logic remains free of Arduino and display-library dependencies so
it can be host-tested.

## Verification

Host tests will cover:

- left/right logical rotation choices;
- absence delay and wake confirmation;
- far, cautious, approaching, hidden, peeking, and retreating transitions;
- 3.0 m and 1.5 m hysteresis boundaries;
- noise smaller than the approach/retreat threshold;
- closed lids during sleep/hide and a narrow slit during peek;
- alert animation profiles selecting squint/anger more frequently and excluding
  crazy/dizzy behavior while approaching;
- `millis()` rollover safety;
- all existing geometry, synchronization, and animation tests.

Firmware verification will include a clean PlatformIO build, upload to the
connected ESP32-C3, UART connection logging, and live serial observation of
state/distance transitions. The final visual thresholds remain tunable after a
physical walk-toward/walk-away test.

## Non-goals

- No radar sensitivity or gate reconfiguration; use the LD2410's current
  configuration.
- No multi-person identity tracking. The nearest reported target drives the
  character.
- No Wi-Fi, Bluetooth, web UI, or persistent settings.
- No backlight switching during sleep or hiding.
