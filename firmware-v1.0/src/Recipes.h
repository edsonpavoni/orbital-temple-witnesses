// Recipes.h — locked PID + trajectory constants. Validated in 04 (production)
// and 03 (diagnostic). Don't change unless you've re-validated on hardware.
#pragma once

#include <Arduino.h>

namespace Recipes {

// Production position-mode recipe — used for move-to-target, spin cruise,
// brake. Tight tracking, ±0.5° accuracy validated by 04_position_lab.
constexpr int32_t PROD_KP    = 30000000;
constexpr int32_t PROD_KI    = 1000;
constexpr int32_t PROD_KD    = 40000000;
constexpr int32_t PROD_MC_MA = 1000;
constexpr float   PROD_MT_PER_DEG_MS = 33.33f;

// Diagnostic position-mode recipe — used during the calibration sweep only.
// Looser loop so gravity-induced lag becomes legible in current draw.
constexpr int32_t DIAG_KP    = 1500000;
constexpr int32_t DIAG_KI    = 30;
constexpr int32_t DIAG_KD    = 40000000;
constexpr int32_t DIAG_MC_MA = 1000;
constexpr float   DIAG_MT_PER_DEG_MS = 50.0f;   // ~5 RPM avg trajectory speed

// Diagnostic sweep parameters.
constexpr float    DIAG_REVS_PER_DIRECTION = 2.0f;
constexpr float    DIAG_FIT_SKIP_FRACTION  = 0.5f;
constexpr uint32_t DIAG_PAUSE_MS           = 1500;

// Tracking-mode setpoint rate cap. Tracker writes the satellite-derived
// target directly to the motor's position-mode loop, which is tuned for
// tight small-delta tracking. A large step (e.g. the first tick after
// enable, when the spin ended at +1080° and the first sample unwraps
// 100°+ away) slams the current cap, peaks above 40 RPM, and oscillates.
// Capping the per-tick setpoint advance smears the catch-up across
// several seconds and keeps the visible motion below SPIN_CRUISE_RPM.
constexpr float    TRACK_MAX_RPM        = 6.0f;

// 1080° spin parameters (BOOT ritual — position-mode rate-integrated, lands
// at exact start angle within ±0.5°. Used once per power-on by SpinOperator
// to bring the pointer to visual zero after homing).
constexpr float    SPIN_TOTAL_DEG       = 1080.0f;
constexpr float    SPIN_CRUISE_RPM      = 8.0f;
constexpr uint32_t SPIN_RAMP_MS         = 2000;
constexpr uint32_t SPIN_BRAKE_GRACE_MS  = 300;
constexpr uint32_t SPIN_SETTLE_MS       = 2000;
constexpr int32_t  SPIN_BRAKE_MAX_MA    = 1500;

// ── 07-style speed-mode spin (used by ChoreoSpin in 16; kept for reference)
// These are also reused by SpeedBrakeSpin for the cruise PID (gravity-alive
// swing aesthetic during the 1800° spin). SPIN07_* during cruise,
// BRAKE_PID_* for the position-mode stop at the end.
constexpr int32_t  SPIN07_KP             = 1500000;     // P=1.5e6 — loose
constexpr int32_t  SPIN07_KI             = 1000;        // I≈0 — don't fight gravity
constexpr int32_t  SPIN07_KD             = 40000000;    // D=4e7 — damp oscillation
constexpr int32_t  SPIN07_MC_MA          = 1000;        // 1000 mA (cruise)
constexpr float    SPIN07_CRUISE_RPM     = 10.0f;       // unused by SpeedBrakeSpin (uses SB_CRUISE_RPM)
constexpr float    SPIN07_TOTAL_DEG      = 1080.0f;     // unused by SpeedBrakeSpin
constexpr float    SPIN07_CRUISE_END_DEG = 1020.0f;     // unused by SpeedBrakeSpin
constexpr uint32_t SPIN07_RAMP_MS        = 2000;        // unused by SpeedBrakeSpin (uses SB_RAMP_MS)
constexpr uint32_t SPIN07_SETTLE_MS      = 500;         // unused by SpeedBrakeSpin (uses SB_SETTLE_MS)

// ── Speed-brake 1800° spin (CHOREOGRAPHY — 17_witness_choreography_smooth)
// Speed-mode cruise (gravity-alive, loose PID) + position-mode brake at end.
// Same gravity aesthetic as 07, but deterministic landing via 09's brake.
// Cycle: ENTER_SAT (8s) → HOLD_SAT (30s) → SPIN (~42s) → SPIN_HOLD (30s) → repeat.
// Total cycle ≈ 8 + 30 + 42 + 30 = ~110 s.
constexpr float    SB_TOTAL_DEG        = 1800.0f;   // 5 full revolutions CW
constexpr float    SB_CRUISE_RPM       = 8.0f;      // speed-mode cruise target
constexpr uint32_t SB_RAMP_MS         = 2000;       // cosine ease 0 -> cruise (ms)
constexpr float    BRAKE_LEAD_DEG      = 3.0f;      // fire brake this many deg early
constexpr int32_t  BRAKE_MAX_MA        = 1500;      // position PID max current at brake
constexpr int32_t  BRAKE_PID_P         = 30000000;  // same as PROD_KP (validated)
constexpr int32_t  BRAKE_PID_I         = 1000;      // same as PROD_KI
constexpr int32_t  BRAKE_PID_D         = 40000000;  // same as PROD_KD
constexpr uint32_t SB_SETTLE_MS        = 500;       // brief PID-convergence window after brake
                                                     // (30s hold is owned by Choreographer)

// ── Choreography cycle parameters ──────────────────────────────────────
// Cycle: ENTER_SAT (8s eased entry) → HOLD_SAT (30s frozen) →
// SPIN (~42s speed-brake) → SPIN_HOLD (30s at brake target) → repeat.
// Total cycle ≈ 8 + 30 + 42 + 30 = ~110 s.
constexpr uint32_t CHOREO_HOLD_MS      = 30000;     // freeze at sat az (pre-spin hold)
constexpr uint32_t CHOREO_SPIN_HOLD_MS = 30000;     // hold at brake landing (post-spin hold)

}  // namespace Recipes
