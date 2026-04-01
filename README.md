# Unitree Go2 Autonomous Navigation Stack

[![ROS 2 Humble](https://img.shields.io/badge/ROS2-Humble-blue)](https://docs.ros.org/en/humble/)
[![Platform](https://img.shields.io/badge/Platform-ARM64%20(Orin)-green)](#)
[![Robot](https://img.shields.io/badge/Robot-Unitree%20Go2-orange)](https://www.unitree.com/go2)

## Overview
This repository provides a comprehensive autonomous navigation and SLAM framework for the **Unitree Go2** quadruped robot. It integrates high-frequency LiDAR-Inertial Odometry, the Nav2 stack, and a custom command bridge. This project serves as the primary technical implementation for a **Master's Thesis** focusing on autonomous quadrupedal robotics and Embodied AI.

---

## System Architecture
The software architecture is designed for high performance and modularity on the **Jetson Orin Expansion Dock**, utilizing a decoupled multi-layer approach:

| Layer | Component | Description |
| :--- | :--- | :--- |
| **Perception** | `hesai_ros_driver_2` | Direct ingestion of PointCloud2 data from the Hesai XT16 LiDAR. |
| **SLAM** | `fast_lio_ros2` | Tightly-coupled LiDAR-IMU fusion via IEKF for robust state estimation. |
| **Navigation** | `Nav2 Stack` | Global and local path planning using `SmacPlannerHybrid` and MPPI controllers. |
| **Executive** | `go2_nav_bridge` | High-level command translation from `cmd_vel` to Unitree `SportModeCmd`. |

---

## Hardware Configuration
To ensure system integrity, the following network topology and hardware specifications are required:

### Network Topology
*   **Expansion Dock (Orin)**: `192.168.123.18` (Primary compute unit, SSH enabled).
*   **Motion Control Unit (MCU)**: `192.168.123.161` (Hardware motor control, SSH blocked).
*   **Hesai XT16 LiDAR**: `192.168.123.20` (UDP Data Port: 2368).

---

## Installation & Setup

### Repository Initialization
Clone the repository and initialize all specialized submodules recursively:
```bash
git clone --recursive https://github.com/ABBranca/go2_ws.git
cd go2_ws
```

### Containerized Environment
The environment is managed via Docker to ensure dependency consistency:
```bash
# Build and start the background services
docker compose up -d --build

# Access the interactive shell
docker exec -it go2_navigation bash
```

---

## Development Workflow

### Synchronization & Compilation
For rapid prototyping, source code should be modified locally and synchronized to the robot:
1.  **Sync**: Execute `./sync_to_dog.sh` to transfer the `src/` directory via `rsync`.
2.  **Build**: Run `colcon build --symlink-install` within the Docker container on the robot.

### Remote Monitoring
Visualization and telemetry should be performed on a remote workstation within the same ROS domain:
```bash
export ROS_DOMAIN_ID=1
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
```

---

## Thesis & Research Objectives
This project investigates the following research areas:
*   **SLAM Benchmarking**: Evaluating the precision of LiDAR-Inertial fusion on dynamic quadrupedal platforms.
*   **Constrained Navigation**: Optimization of hybrid A* and MPPI controllers for non-holonomic quadruped locomotion.
*   **Autonomous Inspection**: Implementation of Vision-Language Models (VLM) for semantic scene understanding and object-specific inspection.

---

## Deployment Standards
For final deployment and immutable production environments:
1.  **Platform Build**: `docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load .`
2.  **Remote Deployment**: `docker save go2_nav_stack:latest | ssh -C unitree@192.168.123.18 'docker load'`

---

**Lead Researcher**: [ABBranca](https://github.com/ABBranca)  
**Affiliation**: University Research Program - Autonomous Systems & Robotics.
