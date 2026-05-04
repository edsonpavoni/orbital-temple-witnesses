// SpeedBrakeSpin.h — 09_speed_brake motion profile, modular class.
//
// Implements the speed-mode cruise + position-mode brake hybrid for a
// 1800° choreography spin. Replaces ChoreoSpin in 17_witness_choreography_smooth.
//
// Profile:
//   RAMP_UP  — speed mode, cosine ease 0 -> SB_CRUISE_RPM over SB_RAMP_MS.
//   CRUISE   — speed mode, hold SB_CRUISE_RPM. Polls encoder every tick.
//              Fires brake when (curEnc - spinStartEnc) >= SB_TOTAL_DEG - BRAKE_LEAD_DEG.
//   BRAKE    — one-shot: PID first, then mode, then max-current, then setpoint.
//              Each write separated by ~5ms. No output toggle (rotor never coasts).
//   SETTLE   — position-mode hold, SB_SETTLE_MS for PID to converge.
//              The 30s SPIN_HOLD afterwards is owned by Choreographer, not here.
//
// Pre/post motor state contract:
//   start(): caller MUST have motor in position mode, output ON (coming from
//            C_HOLD_SAT). start() switches to speed mode WITHOUT toggling output.
//   isActive() == false: motor is in position mode, output ON, setpoint at
//            spinStartEnc + SB_TOTAL_DEG. Choreographer enters C_SPIN_HOLD.
//
// Direction: always CW (+1800°) for now. Direction randomization planned for
// a later iteration once the deterministic profile is validated on hardware.
//
// All motor I/O goes through MotorIO:: — no raw register writes here.
#pragma once

#include <Arduino.h>

class SpeedBrakeSpin {
 public:
  SpeedBrakeSpin() = default;

  // Start a 1800° CW spin. Motor must be in position mode, output ON.
  // Switches to speed mode (no output toggle), captures encoder, begins ramp.
  void  start();

  // Drive the state machine. Call every main-loop iteration; no-op when idle.
  void  tick();

  // True while the spin is in progress (RAMP_UP, CRUISE, BRAKE, or SETTLE).
  bool  isActive() const { return _phase != SP_IDLE; }

  // The absolute encoder position (raw deg) that the brake targets.
  // Valid after start() is called; used by Choreographer for the SPIN_HOLD
  // position reference if needed.
  float targetEnd() const { return _spinTargetEnc; }

 private:
  enum Phase {
    SP_IDLE,
    SP_RAMP_UP,
    SP_CRUISE,
    SP_BRAKE,    // one-shot register sequence; transitions to SETTLE same tick
    SP_SETTLE,
  };

  void fireBrake();   // executes the critical register-order sequence

  Phase    _phase         = SP_IDLE;
  float    _spinStartEnc  = 0.0f;  // raw encoder deg at start()
  float    _spinTargetEnc = 0.0f;  // _spinStartEnc + SB_TOTAL_DEG
  float    _curSpeedRPM   = 0.0f;  // commanded speed during ramp/cruise
  uint32_t _phaseStartMS  = 0;
};
