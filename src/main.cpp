#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GC9A01A.h>
#include <esp_system.h>
#include <ld2410.h>

#include "animation_engine.h"
#include "display_orientation.h"
#include "display_sync.h"
#include "eye_rasterizer.h"
#include "frame_timing.h"
#include "presence_behavior.h"
#include "project_settings.h"
#include "radar_observation.h"

namespace {
constexpr std::uint8_t kTftSclk = 4;
constexpr std::uint8_t kTftMosi = 6;
constexpr std::uint8_t kTftDc = 0;

constexpr std::uint8_t kLeftCs = 7;
constexpr std::uint8_t kLeftReset = 1;
constexpr std::uint8_t kRightCs = 10;
constexpr std::uint8_t kRightReset = 3;

constexpr std::uint8_t kRadarRx = 20;
constexpr std::uint8_t kRadarTx = 21;
constexpr std::uint32_t kRadarBaud = 256000U;
constexpr std::uint8_t kRadarDrainPasses = 4U;

constexpr std::uint32_t kSpiFrequencyHz = 27000000;
constexpr EyeControlPolicy kEyeControlPolicy = eyeControlPolicy();

Adafruit_GC9A01A leftEye(kLeftCs, kTftDc, kLeftReset);
Adafruit_GC9A01A rightEye(kRightCs, kTftDc, kRightReset);
ld2410 radar;
AnimationEngine animationEngine(1U);
PresenceBehavior presenceBehavior(1U);
std::uint32_t lastFrameAt = 0U;
std::uint32_t lastPerformanceReportAt = 0U;
std::uint32_t pendingRadarBytes = 0U;
RadarObservation lastRadarObservation = {false, false, false, 0U, 0U};
PresenceState lastReportedState = PresenceState::Sleeping;
bool lastReportedConnection = false;
bool hasReportedState = false;
alignas(4) std::uint16_t frameBuffer[kEyePixelCount];

void transferFrame(Adafruit_GC9A01A &display,
                   const bool alsoSelectRightEye) {
  display.startWrite();
  if (alsoSelectRightEye) {
    digitalWrite(kRightCs, LOW);
  }
  display.setAddrWindow(0, 0, kEyeWidth, kEyeHeight);
  display.writePixels(frameBuffer, kEyePixelCount);
  if (alsoSelectRightEye) {
    digitalWrite(kRightCs, HIGH);
  }
  display.endWrite();
}

void drawEye(Adafruit_GC9A01A &display, const EyePose &eye,
             const float transitionClosure) {
  composeEyeFrame(eye, transitionClosure, frameBuffer);
  transferFrame(display, false);
}

void drawFrame(const EyePairPose &frame) {
  if (canBroadcastEyeFrame(frame)) {
    composeEyeFrame(frame.left, frame.transitionClosure, frameBuffer);
    transferFrame(leftEye, true);
    return;
  }

  drawEye(leftEye, frame.left, frame.transitionClosure);
  drawEye(rightEye, frame.right, frame.transitionClosure);
}

const char *directionName(const PresenceState state) {
  if (state == PresenceState::Approaching) {
    return "approaching";
  }
  if (state == PresenceState::Retreating) {
    return "retreating";
  }
  return "stable";
}

RadarObservation readRadarObservation(const std::uint32_t nowMs) {
  const std::uint32_t receivedBytes =
      static_cast<std::uint32_t>(Serial1.available());
  for (std::uint8_t pass = 0U; pass < kRadarDrainPasses; ++pass) {
    radar.read();
  }
  const bool connected = radar.isConnected();
  if (!connected) {
    lastRadarObservation.connected = false;
    lastRadarObservation.presence = false;
    lastRadarObservation.movingTarget = false;
    lastRadarObservation.distanceCm = 0U;
    return lastRadarObservation;
  }

  const FrameData frame = radar.getFrameData();
  if (!consumeRadarFrameActivity(pendingRadarBytes, receivedBytes,
                                 frame.length)) {
    lastRadarObservation.connected = true;
    return lastRadarObservation;
  }

  const bool moving = radar.movingTargetDetected();
  const bool stationary = radar.stationaryTargetDetected();
  const std::uint16_t distance = nearestTargetDistanceCm(
      moving, radar.movingTargetDistance(), stationary,
      radar.stationaryTargetDistance());
  lastRadarObservation =
      {true, radar.presenceDetected(), moving, distance, nowMs};
  return lastRadarObservation;
}
}  // namespace

void setup() {
  Serial.begin(115200);
  bool radarStarted = false;
  if (kEyeControlPolicy.initializeRadar) {
    Serial1.begin(kRadarBaud, SERIAL_8N1, kRadarRx, kRadarTx);
    radar.debug(Serial);
    radarStarted = radar.begin(Serial1);
  }

  // Keep both displays deselected while the shared SPI bus is configured.
  pinMode(kLeftCs, OUTPUT);
  pinMode(kRightCs, OUTPUT);
  digitalWrite(kLeftCs, HIGH);
  digitalWrite(kRightCs, HIGH);

  SPI.begin(kTftSclk, -1, kTftMosi);

  leftEye.begin(kSpiFrequencyHz);
  rightEye.begin(kSpiFrequencyHz);
  leftEye.setRotation(displayRotation(EyeDisplay::Left));
  rightEye.setRotation(displayRotation(EyeDisplay::Right));

  animationEngine = AnimationEngine(esp_random());
  if (kEyeControlPolicy.applyPresenceBehavior) {
    presenceBehavior = PresenceBehavior(esp_random());
  }
  lastFrameAt = millis();
  lastRadarObservation.observedAtMs = lastFrameAt;
  const std::uint32_t firstFrameStartedAt = micros();
  EyePairPose firstFrame = animationEngine.update(lastFrameAt);
  if (kEyeControlPolicy.applyPresenceBehavior) {
    const PresenceOutput sleeping = presenceBehavior.update(
        {false, false, false, 0U, lastFrameAt}, lastFrameAt);
    applyPresenceOverlay(firstFrame, sleeping);
  }
  drawFrame(firstFrame);
  const std::uint32_t firstFrameMicros = micros() - firstFrameStartedAt;

  Serial.println("Dual GC9A01 random personality mode ready");
  if (kEyeControlPolicy.initializeRadar) {
    Serial.printf("LD2410 UART GPIO20/RX GPIO21/TX: %s\n",
                  radarStarted ? "connected" : "waiting for data");
  } else {
    Serial.println("LD2410 mmWave behavior disabled by project setting");
  }
  Serial.printf("Initial dual-eye frame: %lu us\n",
                static_cast<unsigned long>(firstFrameMicros));
}

void loop() {
  const std::uint32_t now = millis();
  RadarObservation observation = {false, false, false, 0U, now};
  if (kEyeControlPolicy.pollRadar) {
    observation = readRadarObservation(now);
  }
  if (!isEyeFrameDue(now, lastFrameAt)) {
    return;
  }

  lastFrameAt = now;
  PresenceOutput behavior = {PresenceState::Normal, AnimationProfile::Normal,
                             0.0F, 0.0F, false};
  if (kEyeControlPolicy.applyPresenceBehavior) {
    behavior = presenceBehavior.update(observation, now);
    if (behavior.becameVisible) {
      animationEngine = AnimationEngine(esp_random());
    }
    animationEngine.setProfile(behavior.profile, now);
  }
  EyePairPose frame = animationEngine.update(now);
  if (kEyeControlPolicy.applyPresenceBehavior) {
    applyPresenceOverlay(frame, behavior);
  }

  const std::uint32_t frameStartedAt = micros();
  drawFrame(frame);
  const std::uint32_t frameMicros = micros() - frameStartedAt;
  if (kEyeControlPolicy.applyPresenceBehavior) {
    if (!hasReportedState || behavior.state != lastReportedState ||
        observation.connected != lastReportedConnection) {
    Serial.printf("Radar %s, state %s, distance %u cm, direction %s\n",
                  observation.connected ? "connected" : "disconnected",
                  presenceStateName(behavior.state), observation.distanceCm,
                  directionName(behavior.state));
    lastReportedState = behavior.state;
    lastReportedConnection = observation.connected;
    hasReportedState = true;
    }
  }
  if (now - lastPerformanceReportAt >= 2000U) {
    if (kEyeControlPolicy.applyPresenceBehavior) {
      Serial.printf(
          "Radar %s, state %s, distance %u cm, direction %s; animation %u, "
          "profile %u, dual-eye frame %lu us\n",
          observation.connected ? "connected" : "disconnected",
          presenceStateName(behavior.state), observation.distanceCm,
          directionName(behavior.state),
          static_cast<unsigned>(animationEngine.currentAnimation()),
          static_cast<unsigned>(animationEngine.profile()),
          static_cast<unsigned long>(frameMicros));
    } else {
      Serial.printf("mmWave disabled; animation %u, dual-eye frame %lu us\n",
                    static_cast<unsigned>(
                        animationEngine.currentAnimation()),
                    static_cast<unsigned long>(frameMicros));
    }
    lastPerformanceReportAt = now;
  }
}
