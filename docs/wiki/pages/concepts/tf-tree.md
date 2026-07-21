---
type: concept
tags: [ros2, tf, coordinate-frames]
aliases: [TF tree, TF chain, coordinate frames]
updated: 2026-05-29
---

# TF Tree — Coordinate Frame Chain

## Chain

```
map
 └── odom
      └── base_link
           └── hesai_lidar
```

## Frame Publishers (current — 2D stack, see [[slam-toolbox-2d]])

| Transform | Publisher | Rate |
|-----------|-----------|------|
| `map → odom` | `slam_toolbox` (2D scan-match + loop closure) | ~50 Hz |
| `odom → base_link` | `odom_tf_broadcaster` (Go2 leg odometry `/utlidar/robot_odom`) | odom rate |
| `base_link → hesai_lidar` | Static TF (launch file) | latched |

> ~~Previously `map→odom` and `odom→base_link` were both published by [[fast-lio2]]~~ (deprecated 2026-05-29; FAST-LIO2 also injected the non-REP-105 `camera_init`/`body` frames bridged by static identities — all removed in the 2D stack). See [[z-observability]] for why.

## Static Transform: base_link → hesai_lidar

From [[extrinsics]]:

```
T = [0.171, 0.0, 0.0908] m   (x forward, z up)
R = I₃                        (no rotation)
```

The LiDAR is mounted 17.1 cm forward and 9.08 cm above the body reference point.

## Semantic Meaning

| Frame | Meaning |
|-------|---------|
| `map` | Global fixed frame — SLAM map origin |
| `odom` | Odometry frame — continuous, drift-accumulating |
| `base_link` | Robot body frame — IMU body frame reference |
| `hesai_lidar` | LiDAR optical center |

## Nav2 Dependency

[[nav2]] requires `map → base_link` to be continuously published for costmap updates and planning. Any break in this chain → navigation failure.
