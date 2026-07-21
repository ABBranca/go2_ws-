---
type: concept
tags: [slam, algorithm]
aliases: [SLAM, Simultaneous Localization and Mapping]
updated: 2026-05-29
---

# SLAM — Simultaneous Localization and Mapping

## Current approach (since 2026-05-29): 2D
2D occupancy SLAM via [[slam-toolbox-2d]]: the [[hesai-xt16]] cloud is flattened to a `/scan` and mapped by `slam_toolbox`; the Go2 leg odometry provides `odom→base_link`. Z is **not estimated**.

**Why the change:** [[fast-lio2]] (3D LiDAR-inertial) diverges vertically — the horizontal 16-beam LiDAR cannot observe Z, so the accelerometer bias integrates into a Z runaway. Full root cause: [[z-observability]]. Since [[nav2]] is 2D and the floor is flat, removing Z from the state eliminates the failure by construction.

## ~~Why FAST-LIO2~~ (superseded)
Originally chosen for feature-less robustness, ikd-Tree efficiency, tight IMU coupling, ROS 2 port. **Disqualified on this hardware geometry** (horizontal 16-beam) by the Z runaway. See [[fast-lio2]] → Known Failure.

## Alternatives evaluated (council 2026-05-29)
- **Point-LIO / DLIO** — same 3D-LIO failure mode (Z unobservable). Rejected.
- **LIO-SAM / GLIM** — loop closure *bounds* but does not *fix* Z; GLIM heavy on [[orin-dock]] (was removed). Vendored on branch `feature/lio-sam-experiment`. Go2 IMU is 6-axis → LIO-SAM yaw degrades.
- **lidarslam_ros2** (`rsasaki0109`) — map OK, odometry slightly off; pure-lidar so Z still weak.
- **slam_toolbox 2D** — chosen ([[slam-toolbox-2d]]).

## Output (current)
- `slam_toolbox`: `/map` (OccupancyGrid) + TF `map→odom`
- `odom_tf_broadcaster`: TF `odom→base_link` (see [[tf-tree]])
