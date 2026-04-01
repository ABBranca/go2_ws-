---
name: go2-nav-expert
description: Specialized guidance for Unitree Go2 navigation stack development, including FAST_LIO2 SLAM tuning for Hesai XT16, ROS 2 bridge implementation, and remote telemetry networking. Use when working on Go2 motion control, LiDAR mapping, or troubleshooting communication between the Expansion Dock and a remote laptop.
---

# Go2 Navigation Expert

## Overview
This skill provides procedural knowledge for developing autonomous navigation on the **Unitree Go2** using **ROS 2 Humble**. It bridges the gap between the low-level Unitree SDK and high-level Nav2/FAST_LIO2 stacks.

## Core Capabilities

### 1. SLAM & Sensor Tuning
Expert guidance on configuring **FAST_LIO2** for the **Hesai XT16** LiDAR and Go2's onboard IMU.
- **Reference**: See [fast_lio_tuning.md](references/fast_lio_tuning.md) for IMU noise parameters and extrinsic calibration (T_body_lidar).

### 2. Networking & DDS Topology
Managing the communication bridge between the **MCU (192.168.123.161)** and the **Expansion Dock (192.168.123.18)**.
- **Reference**: See [networking_setup.md](references/networking_setup.md) for CycloneDDS configuration and Wi-Fi bridging strategies.

### 3. Rapid Prototyping Workflow
Procedures for the "Laptop-to-Robot" sync-and-build cycle.
- **Reference**: See [deployment.md](references/deployment.md) for `sync_to_dog.sh`, Docker container access, and `colcon` build commands.

## Decision Tree: Troubleshooting Connectivity

1.  **Can you ping 192.168.123.18?**
    - **No**: Check Ethernet cable, laptop static IP (must be `192.168.123.X`), and RJ45 connection on the Dock.
2.  **Is Docker running?**
    - `ssh unitree@192.168.123.18 "docker ps"`
3.  **Are ROS 2 topics visible on the laptop?**
    - **No**: Ensure `ROS_DOMAIN_ID` matches (default: `1`). Check if `multicast` is allowed or use the unicast configuration in [networking_setup.md](references/networking_setup.md).

## Example Commands

- **Sync and Compile**:
  `./sync_to_dog.sh && ssh unitree@192.168.123.18 "docker exec go2_navigation bash -c 'colcon build --symlink-install && source install/setup.bash'"`
- **Monitor SLAM**:
  `ros2 topic echo /odometry/imu_incremental` (inside Docker)
