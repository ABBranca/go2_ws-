---
type: concept
tags: [slam, lidar, imu]
aliases: [motion undistortion, de-skewing, point cloud distortion]
updated: 2026-05-26
---

# Motion Undistortion

## Problem

Mechanical LiDAR (e.g. [[hesai-xt16]]) acquires one scan over ~100 ms. During this time the robot moves → each point is acquired at a different pose → the scan is geometrically distorted ("skewed").

Without correction, [[slam]] matching degrades significantly, especially at higher velocities.

## Solution

Use [[imu]] pre-integration to estimate the robot trajectory within each scan window, then project all points to a common reference time (usually scan end).

**Precondition:** Per-point timestamps must be correct — each point needs its relative acquisition time within the scan.

## Implementation in FAST-LIO2

[[fast-lio2]] performs undistortion before the iEKF update step:

1. Buffer incoming IMU messages at ~253 Hz
2. When LiDAR scan arrives, pre-integrate IMU over `[t_scan_start, t_scan_end]`
3. For each point `p_i` with timestamp `t_i`, compute intermediate pose → rotate/translate `p_i` to reference frame
4. Output: undistorted scan → point-to-plane iEKF matching

## Critical Dependency: Timestamp Correctness

[[hesai-xt16]] publishes PTP absolute timestamps. Driver converts to relative seconds (`timestamp_unit: 0`). **Bug history:**

- `preprocess.h` read field `time` (wrong) → all per-point timestamps = 0 → undistortion collapsed to identity → fix: field renamed to `timestamp` (commit `82c756e`)
- Velodyne handler: integer truncation of PTP µs → relative offset always 0 (commit `4bc8fcc`)

Both bugs manifested as "motion undistortion failure" — FAST-LIO2 ran but produced drifted/degenerate maps.
