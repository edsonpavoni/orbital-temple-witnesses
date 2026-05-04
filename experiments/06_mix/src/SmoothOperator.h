// SmoothOperator.h — Mode 2: smooth 360° rotation in SPEED mode.
//
// Recipe from 03_motion_test (the "beautiful pendulum swing"):
//
//   SPEED mode (motor's internal speed PID closes the loop)
//   target speed = 8 RPM
//   max current  = 1000 mA
//   eased ramps  = 2000 ms cosine S-curve, 0 -> 8 -> 0 RPM
//   speed PID    = whatever's in motor flash (validated as kp=1.5M ki=1k kd=4M)
//
// Sub-state machine:
//   IDLE
//     │ start360CW()
//     ▼
//   RAMP_UP   (cosine ease  0 → 8 RPM   over 2000 ms)
//     ▼
//   CRUISE    (constant 8 RPM           for 5500 ms — covers ~264°)
//     ▼
//   RAMP_DOWN (cosine ease  8 → 0 RPM   over 2000 ms)
//     │ end of ramp-down: hand control back to PreciseOperator
//     ▼
//   IDLE
//
// Travel math at 8 RPM cruise:
//   each 2 s ramp covers ~48° (avg of cosine = 0.5 × 8 RPM × 6 deg/s × 2 s)
//   ramps cover 96° total, cruise must cover 360° − 96° = 264°
//   cruise time = 264° / (8 × 6 deg/s) = 5500 ms
//   total ≈ 9.5 s for one 360°
#pragma once

#include "MotorIO.h"
#include "PreciseOperator.h"
#include <Arduino.h>

class SmoothOperator {
 public:
  enum class State { IDLE, RAMP_UP, CRUISE, RAMP_DOWN };

  static constexpr float    TARGET_RPM      = 8.0f;
  static constexpr int32_t  MC_MA           = 1000;
  static constexpr uint32_t RAMP_MS         = 2000;
  // Cruise terminates by ANGLE travelled (>= SPIN_TARGET_DEG), not by time.
  // CRUISE_MS is now a safety upper bound — kill the spin if it never reaches
  // the angle target (e.g., motor stalled).
  static constexpr float    SPIN_TARGET_DEG = 1080.0f;   // 3 full revolutions
  static constexpr uint32_t CRUISE_MS       = 90000;     // 90 s safety cap

  // Loose speed PID from 03_motion_test. Written explicitly each
  // rotation because motor flash may have been overwritten by 04/05 tuning.
  static constexpr int32_t  KP_SPEED   = 1500000;
  static constexpr int32_t  KI_SPEED   = 1000;
  static constexpr int32_t  KD_SPEED   = 40000000;

  // SmoothOperator hands control back to PreciseOperator at the end of
  // each rotation, so it needs a reference.
  explicit SmoothOperator(PreciseOperator& precise) : _precise(precise) {}

  // Begin a +360° CW rotation.
  void start360CW();

  // Drive the state machine each loop iteration.
  void tick();

  // Abandon any in-flight rotation (used when a low-voltage gate trips).
  // Resets internal state to IDLE and zeros the queued speed command;
  // does NOT re-engage PreciseOperator — caller handles motor output.
  void release();

  bool isRotating() const { return _state != State::IDLE; }

 private:
  PreciseOperator& _precise;
  State    _state               = State::IDLE;
  uint32_t _phaseStartMS        = 0;
  int32_t  _startEncoderCounts  = 0;
};
