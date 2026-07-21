---
type: entity
tags: [software, slam, ros2, navigation]
aliases: [slam_toolbox, 2D SLAM, mapping_2d, pointcloud_to_laserscan, odom_tf_broadcaster]
updated: 2026-05-29
---

# 2D SLAM Stack — slam_toolbox + Go2 Leg Odometry

The replacement for [[fast-lio2]] (decided 2026-05-29, see [[z-observability]]). Collapses SLAM to a plane so the unobservable vertical axis is never estimated.

## Why (architectural decision)
The horizontal [[hesai-xt16]] cannot observe Z → any 3D LiDAR-inertial estimator runs away vertically ([[z-observability]]). [[nav2]] is 2D and the lab floor is flat, so a 2D SLAM removes Z from the state entirely → the runaway is **structurally impossible**, not merely tuned away. Trade-off: no 3D perception (obstacles below/above the scan slab are not mapped).

## Pipeline
```
[Hesai driver] ─/lidar_points─> [pointcloud_to_laserscan] ─/scan─> [slam_toolbox] ─> /map, TF map→odom
[Go2 leg odom] ─/utlidar/robot_odom─> [odom_tf_broadcaster] ─> TF odom→base_link
```

## Components
| Node | Role | Source |
|------|------|--------|
| `pointcloud_to_laserscan` | flatten 3D cloud → 2D `/scan` (slab in **`base_stabilized`**, IMU-levelled, height 0.0–0.45 m; see [[imu-tilt-compensation]]) | apt `ros-humble-pointcloud-to-laserscan` |
| `slam_toolbox` (async) | 2D scan-matching + loop closure → `/map`, TF `map→odom` | apt `ros-humble-slam-toolbox` |
| `odom_tf_broadcaster` | republish `/utlidar/robot_odom` (Go2 leg odometry) as TF `odom→base_link`; optionally publishes `base_link→base_stabilized` (param `publish_stabilized`) | `src/go2_nav_bridge/src/odom_tf_broadcaster.cpp` (NEW) |
| `scan_preprocessor` | optional ring-select of `/lidar_points` → `/lidar_points_ring` before flatten. Built but **not wired** (decimation regressed, see [[2d-map-tuning]]); host for future de-skew | `src/go2_nav_bridge/src/scan_preprocessor.cpp` (NEW) |

Tuned parameters (offline, 2026-06-03, see [[2d-map-tuning]]): `ceres_loss_function: HuberLoss`,
`scan_buffer_size: 30`, `resolution: 0.03`. Map quality metric = final `map→odom` yaw (drift
absorbed by the matcher); best −2.9°.

Z source = Go2 leg/contact kinematics (anchored to floor), never accelerometer-integrated.

## Files
- Launch: `src/go2_nav_bridge/launch/mapping_2d.launch.py` (live), `mapping_2d_replay.launch.py` (offline)
- Config: `config/slam_toolbox_2d.yaml`, `config/pointcloud_to_laserscan.yaml`
- Nodes: `src/go2_nav_bridge/src/odom_tf_broadcaster.cpp`, `src/go2_nav_bridge/src/scan_preprocessor.cpp`
- See [[tf-tree]] for the resulting 2D frame chain.
- See [[offline-slam-replay]] for developing/tuning this stack from a rosbag without the robot.
- See [[clock-domain-mismatch]] for the bag re-stamp prerequisite, and [[2d-map-tuning]] for the parameter sweep + map-quality results.

## Open risks (to validate in lab)
1. **`/utlidar/robot_odom` dependency** — requires `dog_control` on the [[orin-dock]] Expansion Dock. If absent → fallback: derive odom from `/sportmodestate` (position, body_height) or `robot_localization`.
2. **Scan-slab tilt** — ~~`pointcloud_to_laserscan` projects in `base_link`, which pitches/rolls with the body during gait~~ **RESOLVED 2026-06-03 → [[imu-tilt-compensation]].** The flattener now projects in `base_stabilized`, an **IMU-levelled** frame (`odom_tf_broadcaster` with `tilt_source=imu`, gravity-referenced roll/pitch via a complementary filter). History: levelling from the leg-odometry *attitude* first **REGRESSED** the map (+5.8°→+12.9°, [[2d-map-tuning]]) — that attitude is too noisy/biased (≈−2.6° roll while level). The IMU drives it correctly (gravity is observable, bias-bounded). Empirical gain pending a **walking** bag.
3. Not yet validated on robot. Acceptance test: dynamic-gait walk on an open trajectory with an explicit Z(t) plot (must stay bounded near body height).

## Status
Implemented in working tree (branch `feature/fast-lio2-restore-rep105`), pending build + lab test. [[nav2]] rewiring to 2D (`scan→/scan` LaserScan, map→`/map`) follows once the SLAM layer holds.

<!-- created 2026-05-29: SLAM pivot from FAST-LIO2 to 2D slam_toolbox -->
<!-- updated 2026-06-03: offline validation + tuning — height band 0.0–0.45, Huber/buffer30/res0.03; slab-levelling and ring-decimation tested & regressed; added scan_preprocessor + base_stabilized; see [[2d-map-tuning]], [[clock-domain-mismatch]] -->
<!-- updated 2026-06-03: open-risk #2 resolved — IMU-levelled base_stabilized (tilt_source=imu), target_frame retargeted; see [[imu-tilt-compensation]] -->
