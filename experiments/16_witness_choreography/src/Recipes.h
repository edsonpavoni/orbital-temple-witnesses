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

// ── 07-style speed-mode spin (CHOREOGRAPHY only) ───────────────────────
// Replicates 07_smooth1080's recipe: speed mode, loose PID, ~zero integral.
// Gravity-affected swing aesthetic ("alive but not precise"). End angle is
// non-deterministic by ±5–30° — deliberate. The choreographer's next phase
// re-finds the satellite, so landing precision doesn't matter.
constexpr int32_t  SPIN07_KP             = 1500000;     // P=1.5e6 — loose
constexpr int32_t  SPIN07_KI             = 1000;        // I≈0 — don't fight gravity
constexpr int32_t  SPIN07_KD             = 40000000;    // D=4e7 — damp oscillation
constexpr int32_t  SPIN07_MC_MA          = 1000;        // 1000 mA
constexpr float    SPIN07_CRUISE_RPM     = 10.0f;       // bumped from 8 RPM (2026-05-04 polish)
constexpr float    SPIN07_TOTAL_DEG      = 1080.0f;     // 3 full revolutions
constexpr float    SPIN07_CRUISE_END_DEG = 1020.0f;     // ramp-down adds ~60° of coast at 10 RPM
constexpr uint32_t SPIN07_RAMP_MS        = 2000;        // 2 s eased ramp 0↔cruise
constexpr uint32_t SPIN07_SETTLE_MS      = 500;         // brief pause after ramp-down

// ── Choreography cycle parameters ──────────────────────────────────────
// Cycle: ENTER_SAT (8s eased entry via Tracker) → HOLD_SAT (30s frozen) →
// RELEASE_1 (motor off, pointer free-falls under gravity) → SPIN (~12s @
// 10 RPM) → [no second release] → ENTER_SAT (8s) → ...
// Total cycle ≈ 8 + 30 + 2 + 12 = 52 s.
constexpr uint32_t CHOREO_HOLD_MS        = 30000;       // freeze at sat az (bumped from 10s, 2026-05-04)
constexpr uint32_t CHOREO_RELEASE_MS     = 2000;        // motor OFF window — pointer drops to gravity-down

}  // namespace Recipes
