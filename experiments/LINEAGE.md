# Experiment Lineage

Development history of the First Witness firmware, from the first motion test
to the production choreography loop. Each entry summarizes what that experiment
was for, what it proved or failed, and why we moved past it.

The overall arc: motion aesthetics (03-04) -> calibration (05, 05b) -> unified
loop (06) -> spin precision (07-08-09) -> full production firmware in stages
(09_witness through 14) -> choreography layer (16) -> production v1.0 (17,
now at `../firmware-v1.0/`).

---

## Numbering notes

- No `15` exists. The number was skipped between 14 and 16.
- `5_calibration_b` was a continuation session on calibration that ran
  alongside the main sequence; renamed here to `05b_calibration_redo`.
- Two experiments carry `07`: `07_smooth1080` (1080° position-lock) and
  `07_smooth_1800` (1800° extension). Same control scheme, different scale.
- Two experiments carry `09`: `09_speed_brake` (speed+brake hybrid lab) and
  `09_witness` (first complete unified firmware). Both forked from different
  parents at the same development stage.

---

## 03_motion_test

**What it was:** Pure speed-mode rotation lab. A serial-driven bench for tuning
the motor parameters that make the pointer rotate beautifully: cosine S-curve
velocity profile, 2000 ms transitions, loose PID (KP=1.5e6, KI=1000, KD=4e7),
8 RPM minimum to break static friction.

**What it proved:** The "alive" gravity-affected motion recipe. At loose PID
with near-zero integral, the motor acts as a soft spring guiding direction
rather than a stiff servo. Gravity + the unbalanced pointer provide the
dynamics; the motor provides the slow forcing function. The result reads as a
living swing rather than a robotic crawl.

**Why we moved past it:** Beautiful rotation, terrible stopping accuracy. With
loose speed-mode PID and gravity dominating, the rotor coasts 5-30° past any
commanded deceleration target. Stopping at a specific angle needed a different
control regime.

---

## 04_position_lab

**What it was:** Position-mode landing lab. Companion to 03. A serial-driven
bench for tuning the parameters that make the pointer stop at a specific angle
precisely: closed-loop position mode with a shaped trajectory, eased setpoint
integration, stiff PID (KP=30e6, KI=1000, KD=40e6), 1000 mA.

**What it proved:** The precision-stop recipe. Lands within ±0.5° from any
starting angle across the full 360° range. The key insight: command a position
setpoint that moves slowly (33 ms per degree) rather than slamming the target;
the position PID tracks a slowly moving target rather than fighting a big step
error.

**Why we moved past it:** Lab only. No unified firmware, no calibration, no
memory of what "zero" means. Handed off recipes to the next stages.

---

## 05_calibration

**What it was:** Standalone sensorless gravity homing firmware. On power-on,
runs 2 CW + 2 CCW revolutions at slow speed, detects the sinusoidal gravity
signature in the motor's current draw, fits a sinusoid to find gravity-up,
applies the per-pointer `mass_offset_deg` constant (persisted in NVS), and
moves to true visual vertical.

**What it proved:** Sensorless homing works. From any random starting angle,
the pointer finds visual vertical within ±2° (mean 0.3°) in ~80 s with no
external sensors and no hand-positioning. The two-direction sweep cancels
bearing friction and isolates the pure gravity signal.

**Why we moved past it:** Standalone. The calibration algorithm was good; it
needed to be embedded in a full firmware alongside the motion and tracking
modules.

---

## 05b_calibration_redo

**What it was:** A four-stage deep calibration session (STAGE1 through STAGE4
KNOWLEDGE files) that refined the sinusoid fit, diagnosed and fixed fit-quality
issues, validated the mass_offset_deg determination procedure across multiple
mounting positions, and produced the per-pointer calibration procedure written
into 14's KNOWLEDGE.md.

**What it proved:** The calibration procedure is robust across sessions. The
STAGE files document edge cases (what happens if fit amplitude is too low, how
to spot a noisy sample set, what the DIAG_FIT_SKIP_FRACTION guards against).

**Why we moved past it:** A refinement session, not a separate firmware. Its
findings are baked into the production Calibrator module.

---

## 06_mix

**What it was:** First unified firmware combining both motion regimes: precise
position move to a random target (Mode 1) then smooth speed-mode 360° rotation
(Mode 2), repeated in an autonomous loop. No calibration. User physically
positions pointer at zero before power-on.

**What it proved:** The modular architecture pattern (MotorIO header-only,
PreciseOperator + SmoothOperator classes, thin main.cpp state machine) that
all subsequent firmware inherited. The `lab.py` serial bridge for closed-loop
development was also introduced here.

**Why we moved past it:** No auto-calibration (required manual zero-set on each
power-on). The rotation used speed-mode rate-integration for 360°, which
produced non-deterministic end angles. Moving to a sensorless-calibration boot
and deterministic spin was the next priority.

---

## 07_smooth1080

**What it was:** Isolated test of a 1080° (3-revolution) spin with the
gravity-alive speed-mode PID recipe from 03, followed by a position-mode lock
at the exact starting angle. Command `g` triggers the spin; the pointer should
land within ±0.5° of where it started.

**What it proved:** The position-lock approach (lock setpoint at start+1080°
after speed-mode coast) works for 3 revolutions with ±0.5° accuracy, but only
because 1080° mod 360° = 0 means the end orientation is the same regardless
of the exact landing point. The speed-mode cruise still has non-deterministic
distance (gravity help/fight), but the position PID catches the residual.

**Why we moved past it:** The non-deterministic cruise distance was concerning
for 5-revolution spins where gravity effects accumulate more. Tested the
position-mode rate-integrated approach (08_brake1080) as a comparison.

---

## 07_smooth_1800

**What it was:** Direct extension of 07_smooth1080 to 1800° (5 revolutions).
Added the `f` (free/release) command so the pointer could be released to its
natural gravity rest before each spin test. Used as the baseline for comparing
against the speed+brake hybrid in 09_speed_brake.

**What it proved:** 1800° spin with position-mode rate integration returns to
start within ±1° but the motion is "assisted" (the position PID partially
fights gravity throughout, dampening the living-swing aesthetic). Added
`SPIN_CRUISE_END_DEG = 1752°` as the cruise termination threshold (accounting
for the 48° the cosine ramp-down integrates).

**Why we moved past it:** The 09_speed_brake approach (speed-mode cruise, then
a single position-mode brake at the end) was expected to give better gravity-
alive aesthetics across the full 5 revolutions, at the cost of more positional
uncertainty absorbed by the brake. Testing confirmed this was the right trade.

---

## 08_brake1080

**What it was:** Alternative approach for 1080°: position-mode rate-integrated
cruise (setpoint advances at exactly 8 RPM) plus position-mode brake at the
end. Unlike 07's speed-mode, this mode actively fights gravity during cruise
so the setpoint is always exactly 1080° from start.

**What it proved:** Deterministic stop regardless of gravity phase. 10
consecutive spins: max error 1.70°, mean 0.68°. Motor temperature stable at
48-51°C (less current demand than gravity-runaway speed mode). But the motion
aesthetic is less alive: the pointer is stiffly tracked rather than swinging.

**Why we moved past it:** The stiff assisted motion was judged less interesting
for the artwork than the gravity-alive speed-mode swing. The right call was
speed mode for cruise (beauty) + position mode only for the final brake
(precision). This became 09_speed_brake.

---

## 09_speed_brake

**What it was:** Lab test of the speed+brake hybrid for 1800°: speed-mode
cruise at 8 RPM (loose PID, gravity-alive) until encoder displacement reaches
1797°, then a one-shot inline register sequence fires position-mode brake
at exactly spinStart + 1800°. No output toggle during the mode switch.

**What it proved:** The hybrid works. The brake absorbs momentum cleanly and
the landing is within 1° of target. This is the control architecture that
became SpeedBrakeSpin in v1.0. It also established the critical register write
order (PID first, then mode, then max-current, then setpoint) validated and
documented in the README.

**Why we moved past it:** A lab sketch only. The architecture was promoted to
a named module and embedded in the production firmware lineage starting at
16_witness_choreography.

---

## 09_witness

**What it was:** First complete unified firmware for the First Witness
sculpture. Boot calibration + 1080° production spin + indefinite hold at visual
zero. No networking, no satellite tracking. The sculpture wakes up, finds its
own orientation, spins, and holds. Fully autonomous from any starting position.

**What it proved:** End-to-end ritual validated. Lands within ±0.3° of visual
zero across multiple power-cycle tests from random starting positions.
`mass_offset_deg` persists in NVS through power cycles.

**Why we moved past it:** No satellite tracking. The pointer holds at visual
vertical indefinitely, which is a valid artwork behavior but not the final one.
Adding live azimuth tracking required significant new modules (networking,
schedule fetch, TLE interpolation, Tracker eased-ramp).

---

## 10_witness_modular

**What it was:** Refactor of 09_witness into the module architecture that all
subsequent firmware inherited. 09's monolithic main.cpp was split into:
Calibration + Calibrator (gravity homing), SinusoidFit (least-squares),
MotorState (shared state struct), MoveOperator (eased position moves),
SpinOperator (boot spin), Logger (delta-log), Recipes.h (all constants).

**What it proved:** The module architecture compiles and produces identical
behavior to 09. The refactor was primarily about future extensibility: each
new capability (tracking, networking, provisioning) could be added as a new
module pair with minimal changes to existing code.

**Why we moved past it:** Foundation only. The next iteration adds satellite
tracking.

---

## 11_witness_tracking

**What it was:** Added live satellite tracking to the modular base. New modules:
Tracker (eased cosine ramp to current satellite azimuth, runs every 200 ms),
ScheduleClient (HTTPS fetch of /api/schedule), ScheduleStore (NVS-backed cache
and TLE-interpolated az lookup), Network (Wi-Fi connect/reconnect), PowerGate
(Vin readback and boot gate).

**What it proved:** End-to-end satellite tracking validated with live SUPERVIEW
NEO-2 TLE. Sculpture pointer matched the web visualization within measurement
accuracy. NVS cache means the sculpture keeps pointing correctly offline once
it has seen one schedule.

**Why we moved past it:** Wi-Fi credentials were hard-coded. For a 12-sculpture
production run, a captive portal provisioning system was needed.

---

## 12_witness_polish

**What it was:** Stability and Wi-Fi hardening pass on 11_witness_tracking.
Module set is identical to 11; changes were within existing modules: more
robust reconnect logic, better RSSI logging, tighter error handling on schedule
fetch failure, improved serial command parser.

**What it proved:** Sustained operation without Wi-Fi babysitting. Multiple-hour
runs without manual intervention confirmed the schedule-degradation path (hold
current angle when schedule fails) works correctly.

**Why we moved past it:** Still had hard-coded Wi-Fi credentials. Provisioning
was next.

---

## 13_witness_geo

**What it was:** Added observer geolocation so the sculpture can automatically
determine its lat/lon for satellite az/el calculations. New module: Geolocation
(HTTP GET to an IP geolocation API, parses lat/lon into NVS). Previously the
observer position was hard-coded.

**What it proved:** Automatic geolocation works for a studio context (Williamsburg
Brooklyn). The IP geolocation accuracy (~city level) is more than sufficient
for az calculation — the error is under 0.1° for a satellite at reasonable
elevation.

**Why we moved past it:** Still no captive portal for Wi-Fi credential entry.
Adding Provisioning was the last major infrastructure piece before the
choreography work.

---

## 14_witness_provision

**What it was:** Added Wi-Fi captive portal for field provisioning (Provisioning
module), NVS-backed credential storage (WifiCreds module), and the complete
boot sequence now used in production. This is the direct parent of both
16_witness_choreography and firmware-v1.0.

**What it proved:** Any technician can configure a new sculpture Wi-Fi without
modifying firmware. Connect to the sculpture's AP, enter credentials via the
web form, device reboots and connects. All subsequent sculptures in the series
use this provisioning flow.

**Why we moved past it:** The artwork behavior after boot was still the 09-era
final behavior: calibrate, spin once, hold tracking forever. The choreography
layer (recurring cycle: point at satellite, hold, spin, hold, repeat) was the
next and final development phase.

---

## 16_witness_choreography

**What it was:** Forked from 14, added the Choreographer state machine and
ChoreoSpin module. The sculpture now runs a recurring cycle: eased ramp to
satellite azimuth (ENTER_SAT), hold 30 s (HOLD_SAT), motor-off drop + gravity
fall (RELEASE_1), 07-style 1080° speed-mode spin (SPIN), then loop back.
Random CW/CCW spin direction.

**What it proved:** The basic choreography cycle works. The drop-then-spin
gesture is legible. The ENTER_SAT easing feels right at 8 s. The 30 s hold
gives the sat-pointing gesture enough time to register with visitors. RELEASE_2
(a second drop before re-finding the satellite) was tested and cut as redundant.

**Why we moved past it:** The 1080° ChoreoSpin landed non-deterministically
(±5-30° per spin, by design). Edson wanted a 5-revolution (1800°) spin with a
precise deterministic stop at the satellite azimuth. The speed+brake hybrid
from 09_speed_brake was the right architecture. This became 17 (now
firmware-v1.0).
