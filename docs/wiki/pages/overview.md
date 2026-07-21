---
type: analysis
tags: [architecture, overview]
updated: 2026-07-21
---

# System Overview

## Pipeline

```
Hesai XT-16 (10 Hz)       /lidar_points
       │
       ▼
 pointcloud_to_laserscan  /scan
       │
       ▼
  slam_toolbox            /map, map→odom TF
       │
       ▼
    Nav2                  /cmd_vel
       │
       ▼
 go2_nav_bridge           SportModeCmd (Unitree SDK)
       │
       ▼
   Go2 MCU (192.168.123.161)
```

IMU (`/utlidar/imu`, ~253 Hz) feeds [[imu-tilt-compensation]] in `odom_tf_broadcaster` to level the gait-induced tilt before the cloud is projected into a scan. Go2 leg odometry (`/utlidar/robot_odom`) supplies the `odom→base_link` transform. A horizontal XT16 cannot observe Z, so 3D LiDAR-inertial SLAM was retired — see [[z-observability]].

---

## Components

| Component | Role | Package |
|-----------|------|---------|
| [[hesai-xt16]] | LiDAR sensor | `hesai_ros_driver_2` |
| [[slam-toolbox-2d]] | 2D SLAM (scan → map, map→odom) | `slam_toolbox` (apt) |
| [[nav2]] | Path planning + control | `nav2_bringup` |
| [[go2-nav-bridge]] | Velocity → SportAPI + odom/scan TF | `go2_nav_bridge` |

---

## TF Tree

`map` → `odom` → `base_link` → `hesai_lidar`

slam_toolbox publishes `map→odom`; `odom_tf_broadcaster` publishes `odom→base_link` from leg odometry. Static transform `base_link→hesai_lidar`: T = [0.171, 0, 0.0908] m, R = I₃. See [[tf-tree]] and [[extrinsics]].

---

## Compute

[[orin-dock]] (`192.168.123.18`) runs full stack in Docker (ARM64, multi-stage build). Go2 MCU at `.161` handles motion execution.

---

## Key Interfaces

| Topic | Type | Publisher | Subscriber |
|-------|------|-----------|------------|
| `/lidar_points` | `PointCloud2` | hesai driver | pointcloud_to_laserscan, Nav2 obstacle layer |
| `/scan` | `LaserScan` | pointcloud_to_laserscan | slam_toolbox |
| `/utlidar/imu` | `Imu` | Go2 firmware | odom_tf_broadcaster (tilt leveling) |
| `/utlidar/robot_odom` | `Odometry` | Go2 firmware | odom_tf_broadcaster |
| `/map` | `OccupancyGrid` | slam_toolbox | Nav2 (static layer) |
| `/cmd_vel` | `Twist` | Nav2 | go2_nav_bridge |

---

## Deployment

```bash
./sync_to_dog.sh               # rsync to Orin Dock
docker compose up --build -d   # production (ARM64)
docker compose --profile dev up -d  # dev (volume mounts)
```

DDS config: [[dds-config]] — explicit `eth0`, peers at `.18` and `.222`.
