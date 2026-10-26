# Googly eyes for ESP32-C3

Firmware for a pair of 240 x 240 GC9A01 round displays driven by an ESP32-C3
Super Mini. The displays act as animated eyes with wandering pupils, blinks,
squints, angry expressions, and a handful of short novelty animations.

The project also contains an experimental Hi-Link LD2410 mmWave integration.
That integration is deliberately disabled in the current build. The basic
UART reader and presence state machine are implemented, but the physical
sensing behavior has not been systematically tested or calibrated. Treat the
included distances and timings as starting values. They will need tuning for
the final enclosure, sensor position, room, and audience distance.

## Current status

| Area | Status |
| --- | --- |
| Two GC9A01 displays | Implemented and working |
| Opposite display rotation correction | Implemented |
| Random eye animations and blink transitions | Implemented |
| Shared-frame transfer for synchronized eyes | Implemented |
| LD2410 UART reader | Implemented at a basic level |
| LD2410 behavior state machine | Implemented at a basic level |
| LD2410 installation calibration | Not done |
| mmWave behavior in the current build | Disabled |

With mmWave disabled, the eyes run their normal random animations continuously.

## Hardware

### Required parts

- ESP32-C3 Super Mini
- Two 240 x 240 GC9A01 SPI round display modules
- A USB cable that carries both power and data
- Jumper wire or soldered connections
- A power source capable of supplying the ESP32 and both display backlights

### Optional mmWave parts

- Hi-Link LD2410 presence sensor
- A stable 5 V supply for the sensor

The LD2410 is optional. The displays work without it, and the current firmware
does not initialize its UART because mmWave behavior is disabled.

## Pin mappings

### GC9A01 displays

Both displays share the SPI clock, SPI data, and data/command lines. Each
display must keep its own chip-select and reset connection.

| Display pin | Left eye | Right eye | ESP32-C3 function |
| --- | --- | --- | --- |
| VCC | 3V3 | 3V3 | Display logic power |
| GND | GND | GND | Common ground |
| SCL / SCK | GPIO4 | GPIO4 | Shared SPI clock |
| SDA / MOSI | GPIO6 | GPIO6 | Shared SPI data |
| DC | GPIO0 | GPIO0 | Shared data/command |
| CS | GPIO7 | GPIO10 | Independent chip select |
| RST / RES | GPIO1 | GPIO3 | Independent reset |
| BL / BLK | 3V3 | 3V3 | Backlight power, module dependent |

There is no MISO connection because the firmware only writes to the displays.
Do not connect either CS pin permanently to ground. Both displays would then
receive commands intended for the other controller.

Check the electrical specifications of your particular GC9A01 modules before
connecting the backlights. Some breakout boards include a backlight resistor
or transistor and some do not. Two backlights may also exceed the current that
the Super Mini's 3.3 V regulator can supply. Use a suitable external 3.3 V
supply if necessary, with all grounds connected together.

### Display orientation

The physical displays in this build are mounted in opposite directions:

- The left display is physically rotated 90 degrees counter-clockwise.
- The right display is physically rotated 90 degrees clockwise.

The firmware compensates with Adafruit rotation `1` on the left and rotation
`3` on the right. The animation code still works in a normal 240 x 240 logical
coordinate system.

### LD2410 mmWave sensor

| LD2410 pin | ESP32-C3 Super Mini | Notes |
| --- | --- | --- |
| VCC | 5V / VBUS | Use 5 V, not GPIO or the 3.3 V pin |
| GND | GND | Share ground with the ESP32 and displays |
| TX | GPIO20 | Connects to the ESP32 receive pin |
| RX | GPIO21 | Connects to the ESP32 transmit pin |
| OUT | Not connected | The optional digital output is unused |

The sensor UART is configured for 256000 baud, 8 data bits, no parity, and one
stop bit. The firmware uses `Serial1`; native USB `Serial` remains available
for logs and the PlatformIO monitor.

Connect the radar TX pin to ESP GPIO20 and radar RX to ESP GPIO21. TX and RX
cross between the two devices.

## Firmware modes

The mode is selected at compile time in `platformio.ini`:

```ini
-D GOOGLY_EYES_ENABLE_MMWAVE=0
```

### Autonomous mode: current setting

Set the value to `0` for autonomous animation mode. In this mode the firmware:

- Does not initialize `Serial1` for the LD2410.
- Does not poll or parse radar packets.
- Does not apply sleep, hide, or distance-based expression overlays.
- Runs the normal random eye animations continuously.

### Experimental mmWave mode

Change the value to `1` to compile the LD2410 behavior:

```ini
-D GOOGLY_EYES_ENABLE_MMWAVE=1
```

This enables the UART reader, presence controller, distance filtering, and
eyelid overlays. It does not make the sensor installation production-ready.
Expect to tune it on the assembled project.

## Eye animations

The animation engine currently includes:

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

Calm behaviors have higher selection weights and longer durations. Crazy eyes
and dizzy orbit are short, uncommon accents. Immediate repeats are disabled.
Most changes between expressions happen behind a synchronized blink so the
pupils do not jump visibly between unrelated poses.

There are three animation profiles:

- `Normal` uses the balanced default weights.
- `Cautious` selects suspicious squints more often.
- `Approaching` favors squints and angry glares and excludes crazy and dizzy
  animations.

Only the Normal profile is used while mmWave behavior is disabled.

## Display rendering and timing

Each eye is composed into a 240 x 240 RGB565 framebuffer before it is sent to
the display. This prevents white flashes during blinks and closed-eye poses.

The display bus runs at 27 MHz. The scheduler requests a frame every 60 ms,
which is about 16 frames per second. When both eyes contain the same image, the
firmware selects both CS lines and transfers the framebuffer once. Expressions
with different left and right images require two full transfers and therefore
take longer.

## Experimental mmWave behavior

The LD2410 controller is a first implementation, not a calibrated interaction
model. Its current behavior is:

| Condition | Intended response |
| --- | --- |
| No presence | Close the rendered eyelids and sleep |
| Presence returns | Open the eyes over about 700 ms |
| Person is farther away | Use the Normal animation profile |
| Person is between roughly 1.5 m and 3 m | Use more suspicious expressions |
| Person is moving closer | Favor suspicious and angry expressions |
| Person is closer than roughly 1.5 m | Hide behind closed eyelids |
| Hidden for a few seconds | Open a narrow peek for about 700 ms |
| Person moves away | Come out of hiding |

### Current experimental constants

These values live near the top of `src/presence_behavior.cpp`:

| Setting | Current value |
| --- | --- |
| Missing-presence delay | 3000 ms |
| Observation freshness window | 1500 ms |
| Wake opening time | 700 ms |
| Hide closing time | 350 ms |
| Peek duration | 700 ms |
| Peek interval | Random 3000-5000 ms |
| Trend comparison interval | About 1000 ms |
| Approach/retreat change | 25 cm |
| Hide entry distance | Below 150 cm |
| Hide exit distance | Above 170 cm or a retreat trend |
| Retreat re-hide distance | Below 135 cm after a renewed approach |
| Cautious boundary | 300 cm |
| Return-to-normal boundary | 320 cm |
| Distance filter factor | 0.65 |

Two distinct radar observations are required to wake or enter hiding. A zero
distance is treated as unknown rather than as a target at zero centimetres.
The separate entry and exit distances provide hysteresis around the boundaries.

### What still needs calibration

The LD2410 reports energy in distance gates, and its readings depend heavily on
the room and installation. Walls, furniture, moving fabric, display wiring,
sensor angle, enclosure material, and people outside the intended interaction
area can all affect the result.

Before enabling mmWave behavior for normal use, test and tune at least the
following:

- Sensor position and angle relative to the eyes
- Moving and stationary sensitivity for each LD2410 gate
- Maximum useful detection gate
- The real distance at which Normal, Cautious, Hidden, and Retreating feel right
- Filtering and the 25 cm trend threshold
- Sleep delay and false-presence behavior in an empty room
- Reactions to people walking across the sensor rather than toward it
- Behavior with more than one person in view
- Reliability while asymmetric eye frames take longer to transfer

Start with serial logging enabled. Record the reported distances while someone
stands at known marks, walks toward the project, stops, and walks away. Tune one
group of values at a time. Do not begin by changing all thresholds and sensor
gates together; it becomes difficult to tell which change helped.

The current code uses the nearer non-zero moving or stationary target. It does
not identify individuals or track several people independently.

## Software setup

The project uses PlatformIO with the Arduino framework.

Pinned platform and library requirements are listed in `platformio.ini`:

- Espressif32 platform 6.10.0
- Adafruit GFX Library
- Adafruit GC9A01A
- `ncmreynolds/ld2410` 0.2.x

Install PlatformIO Core or use the PlatformIO extension for Visual Studio Code.
From the project directory, download dependencies and build with:

```sh
pio run
```

## Uploading

Connect the ESP32-C3 over native USB, then run:

```sh
pio run --target upload
```

The checked-in configuration currently uses:

```text
/dev/cu.usbmodem101
```

macOS may assign a different name after a reset or reconnection. Find the
current port with:

```sh
pio device list
```

Then update `upload_port` and `monitor_port` in `platformio.ini`.

## Serial monitor

Open the debug console at 115200 baud:

```sh
pio device monitor
```

In autonomous mode, the firmware prints messages similar to:

```text
mmWave disabled; animation 5, dual-eye frame 59552 us
```

Experimental mmWave mode also reports the radar connection, behavior state,
selected distance, direction classification, animation, and frame time.

## Tests

Hardware-independent behavior is covered by C++ host tests in `test/`. The
tests cover animation selection, geometry, framebuffer rendering, display
synchronization, rotation mapping, frame timing, radar distance normalization,
presence states, rollover-safe timers, and the mmWave enable/disable policy.

For example:

```sh
c++ -std=c++17 -Wall -Wextra -pedantic -Iinclude \
  test/test_animation_engine.cpp src/animation_engine.cpp \
  -o /tmp/googly-eyes-test-animation
/tmp/googly-eyes-test-animation
```

The firmware build remains the integration check for Arduino and the installed
display and radar libraries:

```sh
pio run
```

## Project layout

| Path | Purpose |
| --- | --- |
| `platformio.ini` | Board, port, dependencies, and feature setting |
| `src/main.cpp` | Hardware initialization, main loop, display transfer, and logging |
| `src/animation_engine.cpp` | Animation timing, poses, weights, and transitions |
| `src/eye_rasterizer.cpp` | Composes complete RGB565 eye frames |
| `src/lid_geometry.cpp` | Eyelid shape calculations |
| `src/presence_behavior.cpp` | Experimental distance filter and behavior states |
| `src/radar_observation.cpp` | Normalizes moving and stationary radar targets |
| `include/project_settings.h` | Compile-time mmWave control policy |
| `test/` | Host-side behavior and geometry tests |
| `docs/superpowers/` | Design notes and implementation plans |

## Troubleshooting

### Neither display turns on

- Check 3.3 V and ground first.
- Confirm the module's backlight requirements.
- Make sure GPIO4 is SCK and GPIO6 is MOSI.
- Check that both CS lines are held high during startup.

### Only one display works

- Check the independent CS pins: left GPIO7, right GPIO10.
- Check the independent reset pins: left GPIO1, right GPIO3.
- Do not tie either CS line to ground.
- Swap the display modules to separate a wiring fault from a faulty module.

### An eye is rotated incorrectly

Check the physical mounting before changing animation coordinates. Rotation is
defined in `include/display_orientation.h`: left `1`, right `3`.

### Upload port is missing

Run `pio device list`. The ESP32-C3 native USB port can change between
`usbmodem` names after flashing or reconnecting.

### The LD2410 does nothing

First check `GOOGLY_EYES_ENABLE_MMWAVE`. It is currently `0`, so doing nothing
is expected. If it is set to `1`, verify 5 V power, common ground, crossed TX/RX
wiring, and 256000 baud. Use the serial monitor before changing behavior
thresholds.

### The mmWave reactions feel wrong

That is expected at this stage. The behavior is implemented at a barebones
level but has not been calibrated for the finished installation. Capture real
distance logs, tune the LD2410 gates, and then adjust the thresholds in
`src/presence_behavior.cpp`.

## Safety and power notes

- Disconnect power while changing wiring.
- Do not power the LD2410 from a GPIO pin.
- Confirm whether your display boards safely accept 3.3 V on BL/BLK.
- Use a supply with enough current for both backlights, the ESP32-C3, and the
  radar if it is enabled.
- Connect all grounds together when using more than one supply.
