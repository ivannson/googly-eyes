# Random Eye Animations Design

## Intent

Turn the two GC9A01 displays into a continuously animated pair of expressive
eyes. The firmware should feel lively without external controls: it selects
animations in random order, avoids immediate repeats, and transitions cleanly
between expressions.

## Animation set

The engine implements 13 concrete clips:

1. Idle wander
2. Quick glance
3. Single blink
4. Double blink
5. Independent blink
6. Suspicious squint
7. Angry glare
8. Surprised pupils
9. Cross-eyed
10. Dizzy orbit
11. Sleep and wake
12. Crazy independent eyes
13. Tracking sweep

The approved fourteenth item, random personality mode, is the always-running
scheduler that chooses and coordinates those clips.

## Transitions

A blink is the primary transition. Both eyes close, the next clip is selected
at full closure, and the new pose is revealed as the eyes reopen. This masks
otherwise abrupt changes in pupil position, size, and eyelid geometry.

To avoid excessive blinking, boundaries adjacent to single blink, double
blink, or independent blink use a short eased pose transition instead. The
scheduler also prevents immediate repeats.

## Rendering and timing

- The animation engine is independent of Arduino and display hardware so its
  sequencing and pose math can be tested on the laptop.
- Rendering remains procedural: white eye, black pupil, and black eyelids.
- Both displays share SPI but receive distinct poses for animations that need
  independent movement.
- The main loop is non-blocking and targets 10 frames per second, matching the
  measured 95-96 ms dual-display transfer time of the Adafruit ESP32 driver.
- SPI runs at 40 MHz now that both displays have been confirmed working.
- Each animation runs for its own natural duration before the scheduler picks
  the next one.

## Constraints

- Preserve the existing ESP32-C3 pin mapping.
- Keep the Adafruit GC9A01A and Adafruit GFX libraries.
- Do not require sensors, buttons, networking, or additional hardware.
- Seed random behavior from the ESP32 hardware random source at startup.
