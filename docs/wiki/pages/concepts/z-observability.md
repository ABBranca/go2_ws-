---
type: concept
tags: [slam, imu, sensor, navigation]
aliases: [Z observability, vertical drift, Z runaway, vertical unobservability]
updated: 2026-05-29
---

# Vertical (Z) Observability — Why FAST-LIO2 Diverges

## The failure (lab 2026-05-29)
With [[fast-lio2]] running on the [[hesai-xt16]] mounted **horizontally** on the [[go2-robot]]:
- **Stationary at init:** odometry perfect (drift ~1 cm).
- **After any motion (stand-up or a gentle walk):** the Z position runs away — quadratic ramp, ~0.14 m/s² constant residual acceleration — and never recovers, while X/Y/yaw stay stable. The pose leaves the map → no neighbours → `No Effective Points` → coasts to infinity.

Bag analysis (`/tmp/standup_bag`, `/tmp/walk_bag`): IMU healthy (rad/s, m/s², 252 Hz, peak |ω|=0.4 rad/s), per-point `time` deskew correct, IMU↔LiDAR clock aligned ±2 ms, attitude quaternion sane. **All inputs clean** — only Z diverges.

## Root cause — geometry, not algorithm
FAST-LIO2 kinematic model: `Gv̇ = GR·(a_m − b_a − n_a) + Gg`. The accelerometer bias `b_a` is corrected only when LiDAR residuals constrain the corresponding axis (point-to-plane). The [[hesai-xt16]] has a **30° vertical FOV (±15°), 2° vertical resolution**. Mounted horizontally on a low quadruped, its beams barely graze the **floor** and miss the **ceiling** → almost no **horizontal plane features** → the Z residual cannot be formed → `b_a,z` integrates unchecked → quadratic Z runaway.

> Key: a horizontally rich scene (tables, chairs, boxes — as in our lab) constrains **X/Y/yaw** (vertical surfaces) but **not Z**. Z observability depends on the LiDAR's *vertical sampling*, which a horizontal 16-beam cannot provide. Confirmed by FAST-LIO2 paper + NotebookLM.

## What does NOT fix it
- **Denser feature tuning** (`point_filter_num`, `filter_size_*`): adds horizontal features, not vertical. Tested — no effect.
- **Inflating `b_acc_cov`**: lets the bias wander faster on an unobservable axis — a band-aid, rejected.
- **Swapping to another 3D LIO** (Point-LIO, DLIO): same model, same failure.
- **Loop closure** (LIO-SAM, GLIM): *bounds* drift (sawtooth), does not *fix* the unobservability; fails on open trajectories; injects TF jumps.

## The fix — remove Z from the state (architectural decision)
Z-translation observability is a function of **sensor geometry**. Since [[nav2]] is 2D and the lab floor is flat, the decision (council of 3 agents + NotebookLM, 2026-05-29) is to **collapse the problem to a plane**: see [[slam-toolbox-2d]]. Z is sourced from the Go2 leg-kinematic odometry (bounded to floor), never integrated from the accelerometer.

Tilting the LiDAR 10–15° down (to sweep the floor → gain horizontal planes → make Z observable) is the textbook 3D fix but was **excluded by the user** (no mechanical change).

<!-- created 2026-05-29: lab finding — FAST-LIO2 Z runaway root cause + SLAM pivot rationale -->
