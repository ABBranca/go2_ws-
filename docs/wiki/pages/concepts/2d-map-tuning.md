---
type: analysis
tags: [slam, navigation, config, sensor]
aliases: [2D map tuning, scan tuning, map quality, scan_preprocessor, base_stabilized, ring selection]
updated: 2026-06-03
---

# 2D Map Quality Tuning — Lab Results (2026-06-03)

Offline tuning of [[slam-toolbox-2d]] against the re-stamped bag `slam2d_timefixed`
(see [[clock-domain-mismatch]]), via [[offline-slam-replay]]. Goal: a **finer** map from the
existing [[hesai-xt16]] data, staying strictly 2D ([[z-observability]] forbids any 3D path).

## Quality metric
Beyond visual wall-sharpness, the **final `map→odom` yaw** is the cleanest scalar: it is the
total drift [[slam-toolbox-2d|slam_toolbox]] silently absorbs from the leg-odometry prior over
the run. **Smaller = the front-end registers more reliably.** Also tracked: known-coverage %
(free+occupied / grid) and wall density (occupied / known).

## Lever sweep
Baseline = default 16-ring flatten, `resolution: 0.05`.

| Run | Change | Known cov. | `map→odom` yaw | Verdict |
|-----|--------|-----------|---------------|---------|
| baseline | — | 42.0 % | −17.2° | — |
| **tuned** | `HuberLoss`, `scan_buffer_size 30`, slab 0.0–0.45 m | 55.3 % | +5.8° | ✅ big gain |
| level1 | + gravity-level slab via `base_stabilized` | 52.7 % | +12.9° | ❌ regressed |
| ring | + keep only rings 7,8 (≈±1°) | 48.4 % | −7.7° | ❌ regressed |
| **res03** | tuned + `resolution: 0.03` | 50.3 % | **−2.9°** | ✅ finest |

## What worked
- **`ceres_loss_function: HuberLoss`** — robust kernel down-weights outlier points in the
  sparse 16-beam→2D scan → sharper registration.
- **`scan_buffer_size: 30`** (was 10) — match against more recent scans → tighter local
  consistency.
- **`resolution: 0.03`** (was 0.05) — finer correlation grid → thinner walls, best drift
  (−2.9°). Trade-off: lower known-% (smaller cells need more hits on a sparse sensor).

## What regressed — and why (both informative)
- **Gravity-leveling the scan slab** (publish `base_link→base_stabilized` with roll/pitch
  removed, retarget `pointcloud_to_laserscan` `target_frame`): yaw +5.8°→+12.9°. The **Go2
  leg-odometry *attitude* is too noisy/biased** to de-tilt by (≈ −2.6° roll while standing
  level). De-tilting by a bad attitude injects per-scan rotational jitter. **A levelled frame
  needs the [[imu]], not `/utlidar/robot_odom`** — the leg estimator's X/Y/yaw is a fine odom
  prior, but its roll/pitch is not. (This is [[slam-toolbox-2d]] open-risk #2, now measured.)
- **Ring-decimation to near-horizontal rings 7,8**: known-coverage 55→48 %. The
  `pointcloud_to_laserscan` **height-band already does range-gated ring-selection**: with the
  slab at 0.0–0.45 m and the sensor at +0.09 m, ring 0 (+15°) leaves the band beyond ~1.3 m
  and ring 15 (−15°) contributes only within ~0.34 m. The off-horizontal rings still add
  **short-range coverage**; discarding them loses coverage for no sharpness gain. Ring↔elevation
  measured: ring 0 = +15° … ring 15 = −15°, 2° steps; rings 7/8 straddle 0°.

## Infrastructure added (kept, not all wired)
- `src/go2_nav_bridge/src/odom_tf_broadcaster.cpp` — now also publishes
  `base_link→base_stabilized` (param `publish_stabilized`, roll/pitch removed). **Off the
  active path** (regressed on leg-odom attitude); ready to be driven by the [[imu]] later.
- `src/go2_nav_bridge/src/scan_preprocessor.cpp` (NEW) — ring-select node
  (`/lidar_points → /lidar_points_ring`, params `ring_min`/`ring_max`). Built/installed but
  **not wired** by default (decimation regressed); retained as the host for future per-point
  **de-skew** (`time` field + odom twist, see [[motion-undistortion]]) on *moving* recordings.

## The ceiling
Wall raggedness is bound by **sensor sparsity (16 beams) + near-static slow trajectory + no
loop closure fired** (open path) — not by config. Real next gains, not tunable here:
1. a **loop-closing trajectory** (revisit origin) so the armed loop closure rigidifies the graph;
2. ~~an **IMU-levelled slab** (the `base_stabilized` code, driven by [[imu]])~~ — **IMPLEMENTED 2026-06-03**, see [[imu-tilt-compensation]] (`tilt_source=imu`, complementary filter); awaits a walking bag to quantify the gain over this `−2.9°` baseline;
3. a synced-clock re-record ([[clock-domain-mismatch]]).

## Tooling
`bags/run_2d_replay_test.sh <bag> <out>` (replay+map+TF check), `bags/map_stats.py <stem>`
(occupancy stats), `bags/inspect_cloud.py` (fields/density), `bags/dump_stamps.py`,
`bags/restamp_bag.py`.

## Related
- [[slam-toolbox-2d]] — the stack being tuned
- [[offline-slam-replay]] — replay workflow
- [[clock-domain-mismatch]] — the prerequisite bag fix
- [[z-observability]] — why this stays 2D
- [[motion-undistortion]] — the de-skew path `scan_preprocessor` will host
- [[imu]] — correct source for slab leveling

<!-- created 2026-06-03: lab finding — 2D map tuning sweep; Huber+buffer+res0.03 win, leveling/ring-decimation regress -->
