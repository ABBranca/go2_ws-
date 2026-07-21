---
type: analysis
tags: [architecture, overview]
updated: 2026-05-26
---

# System Overview

## Pipeline

```
Hesai XT-16 (10 Hz)    /lidar_points
       │
       ▼
  FAST-LIO2            /Odometry, /cloud_registered, map→odom TF
       │
       ▼
    Nav2               /cmd_vel
       │
       ▼
 go2_nav_bridge        SportModeCmd (Unitree SDK)
       │
       ▼
   Go2 MCU (192.168.123.161)
```

IMU (`/utlidar/imu`, ~253 Hz) feeds directly into [[fast-lio2]] for motion undistortion and state propagation.

---

## Components

| Component | Role | Package |
|-----------|------|---------|
| [[hesai-xt16]] | LiDAR sensor | `hesai_ros_driver_2` |
| [[fast-lio2]] | LiDAR-inertial SLAM | `fast_lio_ros2` |
| [[nav2]] | Path planning + control | `nav2_bringup` |
| [[go2-nav-bridge]] | Velocity → SportAPI | `go2_nav_bridge` |

---

## TF Tree

`map` → `odom` → `base_link` → `hesai_lidar`

FAST-LIO2 publishes `map→odom`. Static transform `base_link→hesai_lidar`: T = [0.171, 0, 0.0908] m, R = I₃. See [[tf-tree]] and [[extrinsics]].

---

## Compute

[[orin-dock]] (`192.168.123.18`) runs full stack in Docker (ARM64, multi-stage build). Go2 MCU at `.161` handles motion execution.

---

## Key Interfaces

| Topic | Type | Publisher | Subscriber |
|-------|------|-----------|------------|
| `/lidar_points` | `PointCloud2` | hesai driver | FAST-LIO2 |
| `/utlidar/imu` | `Imu` | Go2 firmware | FAST-LIO2 |
| `/Odometry` | `Odometry` | FAST-LIO2 | Nav2 |
| `/cloud_registered` | `PointCloud2` | FAST-LIO2 | Nav2 (OctoMap) |
| `/cmd_vel` | `Twist` | Nav2 | go2_nav_bridge |

---

## Deployment

```bash
./sync_to_dog.sh               # rsync to Orin Dock
docker compose up --build -d   # production (ARM64)
docker compose --profile dev up -d  # dev (volume mounts)
```

DDS config: [[dds-config]] — explicit `eth0`, peers at `.18` and `.222`.
