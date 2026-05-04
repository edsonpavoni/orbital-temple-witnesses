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

// 1080° spin parameters.
constexpr float    SPIN_TOTAL_DEG       = 1080.0f;
constexpr float    SPIN_CRUISE_RPM      = 8.0f;
constexpr uint32_t SPIN_RAMP_MS         = 2000;
constexpr uint32_t SPIN_BRAKE_GRACE_MS  = 300;
constexpr uint32_t SPIN_SETTLE_MS       = 2000;
constexpr int32_t  SPIN_BRAKE_MAX_MA    = 1500;

}  // namespace Recipes
