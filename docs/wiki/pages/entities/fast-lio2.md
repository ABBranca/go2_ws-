---
type: entity
tags: [software, slam, ros2]
aliases: [FAST-LIO2, FastLIO2]
updated: 2026-05-29
---

# FAST-LIO2 — Fast LiDAR-Inertial Odometry

> ⚠️ **Deprecated as SLAM core (2026-05-29).** Replaced by [[slam-toolbox-2d]] due to an unrecoverable vertical (Z) runaway — see *Known Failure* below and [[z-observability]]. Kept for reference / possible future use if the LiDAR is ever tilted.

## Overview

FAST-LIO2 (Xu et al., 2022, IEEE RA-L) was the SLAM core of this stack. Tightly-coupled LiDAR-inertial odometry using an iterated Extended Kalman Filter (iEKF) with an ikd-Tree for efficient map management. Produces 6-DoF pose estimate at LiDAR scan rate (10 Hz).

## Algorithm Summary

1. **IMU propagation** — high-rate (~253 Hz) IMU pre-integration between LiDAR scans
2. **Motion undistortion** — de-skew [[hesai-xt16]] scan using propagated IMU trajectory
3. **Feature-less matching** — raw points matched against ikd-Tree map (no feature extraction)
4. **iEKF update** — point-to-plane residuals, iterated until convergence (max 3 iterations)
5. **Map update** — ikd-Tree incrementally updated with new points

## ROS 2 Interface

| Topic | Direction | Type |
|-------|-----------|------|
| `/lidar_points` | subscribe | `PointCloud2` |
| `/utlidar/imu` | subscribe | `Imu` |
| `/Odometry` | publish | `Odometry` |
| `/cloud_registered` | publish | `PointCloud2` |
| `/path` | publish | `Path` |

TF published: `map` → `odom` (see [[tf-tree]])

## Configuration

Full parameter reference: [[fast-lio2-params]]  
Extrinsics: [[extrinsics]]  
IMU source: [[imu]]

## Package

`src/fast_lio_ros2/`  
Main node: `src/fast_lio_ros2/src/laserMapping.cpp`  
Config: `src/fast_lio_ros2/config/hesai_xt16.yaml`

## Known Issues & Fixes

| Issue | Fix | Commit |
|-------|-----|--------|
| IMU starvation — buffer never filled | `imu_topic` `/lidar_imu` → `/utlidar/imu` (valid, kept) | `82c756e` |
| Per-point time field mismatch (`Failed to find match for field 'timestamp'`) | ~~register field as `timestamp` FLOAT64 (`82c756e`/`4bc8fcc`) — **regression**, assumed ROS1 driver~~ → reverted to `(float, time, time)` matching the **ROS2** Hesai driver (`source_driver_ros2.hpp` emits `time` FLOAT32 relative offset) | fix 2026-05-29 |
| PTP timestamp truncation | superseded by the above; ROS2 driver uses relative offset, not absolute epoch | — |

## Known Failure — Vertical (Z) runaway (the deprecation cause)

Stationary: odometry fine. After **any motion** (stand-up, gentle walk): Z position diverges quadratically (~0.14 m/s² residual accel), never recovers, while X/Y/yaw stay stable. Root cause is **sensor geometry**, not a bug: the horizontal [[hesai-xt16]] (30° vertical FOV) provides no horizontal plane features → the iEKF cannot constrain Z / correct the accel bias → `b_a,z` integrates. Full analysis + decision: [[z-observability]]. Tuning (`filter_size`, `point_filter_num`) and `b_acc_cov` inflation do **not** fix it. Resolution: pivot to [[slam-toolbox-2d]].
