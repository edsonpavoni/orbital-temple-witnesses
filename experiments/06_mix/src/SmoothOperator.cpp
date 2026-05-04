#include "SmoothOperator.h"

using namespace MotorIO;

void SmoothOperator::start360CW() {
  // Caller (main.cpp's state machine) ensures we're not already rotating.
  // Switch to SPEED mode (jolt-safe pattern: disable -> change mode ->
  // write speed PID -> start at 0 RPM -> re-enable). Writing the PID
  // explicitly each rotation: motor flash may carry tuning from 04/05.
  writeReg8(REG_OUTPUT, 0);
  delay(20);
  writeReg8(REG_MODE, MODE_SPEED);
  delay(20);
  writeReg96(REG_SPEED_PID, KP_SPEED, KI_SPEED, KD_SPEED);
  delay(20);
  writeReg32(REG_SPEED_MAXCUR, MC_MA * 100);
  delay(10);
  writeReg32(REG_SPEED, 0);    // start velocity = 0
  delay(10);
  writeReg8(REG_OUTPUT, 1);

  _startEncoderCounts = encoderRaw();
  _phaseStartMS       = millis();
  _state              = State::RAMP_UP;
}

void SmoothOperator::tick() {
  if (_state == State::IDLE) return;

  uint32_t elapsed = millis() - _phaseStartMS;

  switch (_state) {
    case State::RAMP_UP: {
      if (elapsed >= RAMP_MS) {
        // Snap to exactly TARGET_RPM and start cruising.
        writeReg32(REG_SPEED, static_cast<int32_t>(TARGET_RPM * 100.0f));
        _phaseStartMS = millis();
        _state        = State::CRUISE;
      } else {
        float t   = static_cast<float>(elapsed) / static_cast<float>(RAMP_MS);
        float rpm = TARGET_RPM * easeInOut(t);
        writeReg32(REG_SPEED, static_cast<int32_t>(rpm * 100.0f));
      }
      break;
    }

    case State::CRUISE: {
      // Hold velocity command at TARGET_RPM. Motor's loose speed PID + the
      // unbalanced pointer's gravity dynamics produce the pendulum-swing feel.
      // Terminate by angular travel: when the rotor has covered SPIN_TARGET_DEG
      // (cumulative encoder), begin ramp-down. CRUISE_MS is a safety cap.
      writeReg32(REG_SPEED, static_cast<int32_t>(TARGET_RPM * 100.0f));
      float traveled = static_cast<float>(encoderRaw() - _startEncoderCounts) /
                       static_cast<float>(COUNTS_PER_DEG);
      if (traveled >= SPIN_TARGET_DEG || elapsed >= CRUISE_MS) {
        _phaseStartMS = millis();
        _state        = State::RAMP_DOWN;
      }
      break;
    }

    case State::RAMP_DOWN: {
      if (elapsed >= RAMP_MS) {
        // Velocity command -> 0. Brief settle, then hand control back.
        writeReg32(REG_SPEED, 0);
        delay(200);
        // Switch motor back to POSITION mode at the current encoder reading
        // and re-engage PreciseOperator. From PreciseOperator's perspective,
        // currentTargetDeg is now ~360° larger (one full revolution past
        // where it was) but the pointer is at the same physical angle.
        _precise.reEngageAfterSpeedMode();
        _state = State::IDLE;
      } else {
        float t   = static_cast<float>(elapsed) / static_cast<float>(RAMP_MS);
        float rpm = TARGET_RPM * (1.0f - easeInOut(t));
        writeReg32(REG_SPEED, static_cast<int32_t>(rpm * 100.0f));
      }
      break;
    }

    case State::IDLE:
      break;
  }
}

void SmoothOperator::release() {
  // Zero the speed command so no residual velocity is queued when the motor
  // gets re-enabled later, and reset our state so tick() becomes a no-op.
  writeReg32(REG_SPEED, 0);
  _state = State::IDLE;
}
