# Unitree Go2 Autonomous Navigation Stack

[![ROS 2 Humble](https://img.shields.io/badge/ROS2-Humble-blue)](https://docs.ros.org/en/humble/)
[![Platform](https://img.shields.io/badge/Platform-ARM64%20(Orin)-green)](#)
[![Robot](https://img.shields.io/badge/Robot-Unitree%20Go2-orange)](https://www.unitree.com/go2)

Autonomous navigation framework for the **Unitree Go2** quadruped robot, integrating LiDAR-Inertial SLAM (FAST-LIO2), Nav2, and a custom command bridge. Bachelor's thesis project in Mechatronics Engineering at **UNIMORE**.

| Requirement | Version / Value |
| :--- | :--- |
| ROS 2 | Humble Hawksbill |
| Platform | ARM64 — Jetson Orin (Expansion Dock) |
| Container | Docker + `docker compose` |
| LiDAR | Hesai XT16 |
| IMU | BMI088 (onboard Go2) |

---

## Architecture

The LiDAR point cloud is fused with IMU data by FAST-LIO2, which provides localization and mapping. Nav2 plans paths on the resulting map, and `go2_nav_bridge` translates velocity commands into Unitree's proprietary `SportModeCmd` protocol.

```
Hesai XT16 (UDP:2368)
    └─ hesai_ros_driver_2  →  /lidar_points
                                    │
                               FAST-LIO2  (IEKF + ikd-Tree)
                                    │
                TF: map → odom → base_link → hesai_lidar
                                    │
                               Nav2 Stack
                                    │
                      go2_nav_bridge (cmd_vel → SportModeCmd)
                                    │
                              MCU 192.168.123.161
```

---

## Packages

| Package | Type | Upstream | Role |
| :--- | :--- | :--- | :--- |
| `go2_nav_bridge` | local | — | `cmd_vel` → `SportModeCmd` translation, velocity clamping, watchdog |
| `unitree_ros2` | submodule | [unitreerobotics/unitree_ros2](https://github.com/unitreerobotics/unitree_ros2) | Unitree SDK message types |
| `fast_lio_ros2` | submodule | [hku-mars/FAST_LIO](https://github.com/hku-mars/FAST_LIO) | Tightly-coupled LiDAR-IMU SLAM |
| `hesai_ros_driver_2` | submodule | [HesaiTechnology/HesaiLidar_ROS2](https://github.com/HesaiTechnology/HesaiLidar_ROS2) | Hesai XT16 ROS 2 driver |
| `livox_ros_driver2` | submodule | [Livox-SDK/livox_ros_driver2](https://github.com/Livox-SDK/livox_ros_driver2) | Custom message types required by FAST-LIO2 (no Livox hardware needed) |

---

## Network Topology

| Unit | IP | SSH | Role |
| :--- | :--- | :--- | :--- |
| Expansion Dock (Orin) | `192.168.123.18` | ✅ | Primary compute, Docker + ROS 2 |
| MCU | `192.168.123.161` | ❌ | Motors, low-level SDK |
| Hesai XT16 | `192.168.123.20` | — | UDP `2368` data, TCP `9347` PTC |
| Developer Laptop | `192.168.123.10` | — | Editing, Rviz2 |

---

## Setup

```bash
# Clone and initialize submodules
git clone --recursive https://github.com/ABBranca/go2_ws.git
cd go2_ws
```

> `livox_ros_driver2/package.xml` is already committed to the repo — no manual symlink required.

---

## Development Workflow

### Local (VS Code Dev Containers)
Open the project root in VS Code and run `Dev Containers: Reopen in Container`.
The container includes all ROS 2 dependencies; use the **ROS 2 extension** for IntelliSense and builds,
or the integrated terminal:
```bash
colcon build --symlink-install --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble
source install/setup.bash
```

### Hardware testing (sync → build → visualize)
```bash
# 1. Sync source to robot
./sync_to_dog.sh

# 2. Shell into the container on the robot and build
docker exec -it go2_navigation bash
colcon build --symlink-install --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble
source install/setup.bash

# 3. Visualize on the laptop
export ROS_DOMAIN_ID=1
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
```

---

## Deployment

Requires `docker buildx` with QEMU configured for ARM64 cross-compilation (see [Docker multi-platform docs](https://docs.docker.com/build/building/multi-platform/)).

```bash
# Build ARM64 image locally
docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load .

# Transfer to robot and start
docker save go2_nav_stack:latest | ssh -C unitree@192.168.123.18 'docker load'
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose up -d"
```

For iterative development, skip the image build and use the sync workflow above.

---

## ROS Interfaces

Key topics published/subscribed by the stack:

| Topic | Type | Direction | Publisher |
| :--- | :--- | :--- | :--- |
| `/lidar_points` | `sensor_msgs/PointCloud2` | pub | `hesai_ros_driver_2` |
| `/Odometry` | `nav_msgs/Odometry` | pub | `fast_lio_ros2` |
| `/tf`, `/tf_static` | `tf2_msgs/TFMessage` | pub | `fast_lio_ros2`, `static_transform_publisher` |
| `/cmd_vel` | `geometry_msgs/Twist` | sub | Nav2 → `go2_nav_bridge` |
| `/sportmodestate` | `unitree_go/SportModeState` | pub | MCU |

---

## Known Issues

| Issue | Status | Notes |
| :--- | :--- | :--- |
| Container exits immediately (Exit Code 0) | ✅ Fixed | `docker-compose.yml` now sets `stdin_open: true` and `tty: true` |
| `livox_ros_driver2` build fails | ✅ Fixed | `package.xml` committed; colcon passes `-DROS_EDITION=ROS2 -DHUMBLE_ROS=humble` |
| PCL not found during build | ✅ Fixed | `libpcl-dev` and `ros-humble-pcl-ros` added to `Dockerfile` |
| CycloneDDS random interface selection | ✅ Fixed | `docker/cyclonedds.xml` with `eth0` mounted via `CYCLONEDDS_URI` |
| MCU does not bridge DDS to Wi-Fi | ⏳ Open | USB Wi-Fi dongle on Dock (Alfa AWUS036ACM) or travel router recommended |

---

## Citation

If you use this work, please cite:

```bibtex
@software{go2_nav_stack,
  author  = {Branca, A.},
  title   = {Unitree Go2 Autonomous Navigation Stack},
  year    = {2026},
  url     = {https://github.com/ABBranca/go2_ws},
}
```

---

**Lead Researcher:** [ABBranca](https://github.com/ABBranca) — UNIMORE, Bachelor's Thesis in Mechatronics Engineering
