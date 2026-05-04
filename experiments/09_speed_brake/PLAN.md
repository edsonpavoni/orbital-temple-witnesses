# PLAN.md - 09_speed_brake

## What this test is checking

The Orbital Witnesses production firmware needs a spin profile that:
1. Feels alive -- not mechanically assisted. Gravity should be free to act on
   the unbalanced pointer throughout the cruise.
2. Returns to the starting angle reliably -- within ~1-2 deg across repeated
   runs, so the pointer lands at its gravity-rest position every time.

`07_smooth_1800` achieves goal 2 very well (position PID corrects throughout)
but may compromise goal 1 (the PID partially fights gravity continuously).

`09_speed_brake` tests whether we can achieve both:
- Speed mode for the full cruise (goal 1: gravity-alive)
- Position-mode brake fires 3 deg before the target (goal 2: precision landing)

The output of this test is a data-driven answer to that question.

## What success looks like

Run `f` then `g` at least 5 times. Compare:

| Metric | Acceptable | Good | Excellent |
|--------|-----------|------|-----------|
| `phys-offset-from-start` | < 5 deg | < 2 deg | < 0.5 deg |
| Run-to-run spread (max - min) | < 5 deg | < 2 deg | < 1 deg |

If results are acceptable or better: speed-brake is viable for production.
If results are poor: increase `BRAKE_LEAD_DEG` (fires earlier, more PID time)
or fall back to `07`'s full position-mode approach.

## What failure looks like

- `phys-offset-from-start` consistently > 5 deg: brake fires too late or
  momentum is too high for the PID to absorb in SETTLE_MS.
- Large run-to-run spread (> 5 deg): gravity during cruise introduces
  enough variability that the encoder displacement threshold fires at different
  angular velocities each time.
- Motor cogging / stall during ramp-up: speed mode at 8 RPM with 1000 mA may
  be insufficient for the unbalanced pointer at certain angles. Watch for
  `a=` values plateauing below 8 RPM in the log.

## Tuning knobs (in order of impact)

1. `BRAKE_LEAD_DEG` (default 3.0) -- increase if landing consistently overshoots.
   Each +1 deg gives the PID ~8 ms more at 8 RPM cruise. Try 5, 10, 20.
2. `BRAKE_MAX_MA` (default 1500) -- increase if the PID can't hold against
   gravity at the settled position. Ceiling is 2000 mA.
3. `SETTLE_MS` (default 5000) -- increase if the position PID needs more
   convergence time. 5 s should be sufficient at these gains.

## Comparison baseline

`07_smooth_1800` FINDINGS.md shows a prior run with delta = 1802.8 deg
(encoder from 9171.00 to 10973.80), confirming the encoder reports unwrapped
multi-turn positions as a linear value. The same unwrapping assumption is used
in `09_speed_brake`. If the encoder unexpectedly wraps during a run, the
`encMoved` value will drop sharply negative mid-cruise -- abort with `x` and
investigate.

## Log parsing

The `SB` prefix in all phase markers makes it trivial to filter 09 runs from
07 runs in combined log files:

```
grep "SPIN_SB" serial_log.txt
```

Key values to extract per run:
- `start enc=` -- origin angle
- `brake fire (encMoved=` -- actual displacement at brake trigger
- `done. final=` -- landing angle
- `err=` -- distance from exact target (accumulated float + timing error)
- `phys-offset-from-start=` -- the key metric (should be near 0)
