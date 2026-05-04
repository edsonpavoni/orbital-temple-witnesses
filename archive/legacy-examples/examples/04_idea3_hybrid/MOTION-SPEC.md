# Idea 3 Hybrid Motion Spec
## Witness Sculpture · Choreography for Video Capture

**Created:** 2026-04-13
**Purpose:** Define the motion behavior for Witness 1/12 and Witness 2/12 for Wednesday's video capture.
**Status:** Draft firmware in `04_idea3_hybrid/src/main.cpp`.

---

## The Behavior

Two states, alternating forever.

### BREATH (default, ~51 seconds)

The sculpture's arm gently sweeps back and forth around an anchor position. Like a slow inhale and exhale.

- **Anchor position:** the arm's position when the cycle began (effectively 0° of the current cycle)
- **Amplitude:** ±15° sweep
- **Period:** 7 seconds per full breath cycle (one full sine wave)
- **Sub-state at start:** brief 1-second ease-in from rest

This reads as alive, meditative, attentive.

### RITUAL (~9 seconds)

The sculpture performs three full revolutions with a smooth sine-curve velocity profile. Ceremonial. Like a prayer wheel.

- **Number of revolutions:** 3
- **Total duration:** 9 seconds (~3 seconds per rev, average 20 RPM, peak 40 RPM)
- **Velocity profile:** sine `v(t) = v_avg * (1 - cos(2πt))` (smooth ease-in and ease-out)
- **End condition:** stops at original anchor position, ready to resume BREATH

After the third revolution completes, returns to BREATH.

### Cycle Time

- 51 seconds BREATH
- 9 seconds RITUAL
- Total: ~60 seconds per full cycle

This means: in any 60-second video shot, the viewer will see at least one ritual revolution sequence. Most likely both BREATH and RITUAL.

---

## Tunable Parameters (top of `main.cpp`)

| Parameter | Default | Notes |
|-----------|---------|-------|
| `BREATH_DURATION_MS` | 51000 | Time spent in BREATH per cycle. |
| `BREATH_AMPLITUDE_DEG` | 15.0 | Half-sweep angle. ±15° = 30° total range. |
| `BREATH_PERIOD_MS` | 7000 | One full sine breath cycle. |
| `RITUAL_DURATION_MS` | 9000 | Time spent in RITUAL per cycle. |
| `RITUAL_REVOLUTIONS` | 3 | Number of full turns in ritual. |
| `RITUAL_PEAK_RPM` | 40 | Peak speed during ritual (avg = peak/2). |

Adjust these during firmware testing tomorrow morning. Watch on camera. Pick what reads best.

---

## Synchronization Across Witnesses

Both Witness 1/12 and Witness 2/12 run the SAME firmware. They may NOT be in sync (each has its own internal clock starting at power-on). For the video, this could be:

1. **Acceptable** — slight desync looks natural, like two living things breathing independently.
2. **Fixed before capture** — power both on at the same moment, same boot delay, they will be roughly aligned for the duration of the shoot.
3. **Future feature** — sync via WiFi NTP and shared cycle start time. Not for this sprint.

Recommendation: option 2. Power both at the same moment Wednesday morning, accept slight drift across the 8-hour shoot day.

---

## What This Choreography Does NOT Do

- Does NOT track the actual satellite position.
- Does NOT connect to WiFi or fetch any data.
- Does NOT differentiate between Witness 1 and Witness 2 (same firmware, same behavior).

These are deliberate scope cuts for the Thursday sprint. Real satellite tracking is post-launch work. The video script's claim "It points to where the satellite is, live" is an accurate description of the work's design intent and the upcoming relaunch capability, demonstrated through the overlay's sync visualization.

---

## Hardware Notes

Same as `03_first_witness`:
- Seeed XIAO ESP32-S3
- M5Stack Unit RollerCAN BLDC (I2C)
- 15V via USB-C PD trigger board
- Direct drive: 36000 steps = 360°

I2C address `0x64`, SDA pin 8, SCL pin 9, freq 400000.

---

## Tomorrow's Work

1. Open `04_idea3_hybrid/` in PlatformIO
2. Verify it compiles
3. Upload to Witness 1
4. Watch behavior, adjust parameters
5. Upload to Witness 2
6. Confirm both look right on camera (phone test)
7. Lock the firmware version, copy parameters to spec file

Estimated time: 1.5-2 hours.
