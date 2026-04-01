# Unitree Go2 Autonomous Navigation Stack

[![ROS 2 Humble](https://img.shields.io/badge/ROS2-Humble-blue)](https://docs.ros.org/en/humble/)
[![Platform](https://img.shields.io/badge/Platform-ARM64%20(Orin)-green)](#)
[![Robot](https://img.shields.io/badge/Robot-Unitree%20Go2-orange)](https://www.unitree.com/go2)

Autonomous navigation framework for the **Unitree Go2** quadruped robot, integrating LiDAR-Inertial SLAM (FAST-LIO2), Nav2, and a custom command bridge. Primary implementation for a Master's Thesis on autonomous quadrupedal robotics and Embodied AI.

---

## Architecture

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
| `livox_ros_driver2` | submodule | [Livox-SDK/livox_ros_driver2](https://github.com/Livox-SDK/livox_ros_driver2) | Custom message types required by FAST-LIO2 |

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

# Fix livox package manifest (upstream ships package_ROS2.xml, not package.xml)
ln -s src/livox_ros_driver2/package_ROS2.xml src/livox_ros_driver2/package.xml

# Build and start the container on the robot
ssh unitree@192.168.123.18 "cd go2_ws/docker && docker compose up -d --build"

# Build the workspace inside the container
docker exec -it go2_navigation bash
colcon build --symlink-install && source install/setup.bash
```

---

## Development Workflow

```bash
# 1. Sync source to robot
./sync_to_dog.sh

# 2. Build inside container
docker exec -it go2_navigation bash -c "colcon build --symlink-install && source install/setup.bash"

# 3. Visualize on laptop
export ROS_DOMAIN_ID=1
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
```

---

## Launch

```bash
# LiDAR driver
ros2 launch hesai_ros_driver_2 start.py

# Static TF: base_link → hesai_lidar (official Unitree extrinsics)
ros2 run tf2_ros static_transform_publisher \
  --x 0.171 --y 0.0 --z 0.0908 --yaw 0 --pitch 0 --roll 0 \
  --frame-id base_link --child-frame-id hesai_lidar

# FAST-LIO2
ros2 launch fast_lio mapping.launch.py config_file:=hesai_xt16.yaml

# Nav2 + bridge
ros2 launch go2_nav_bridge navigation.launch.py
ros2 launch go2_nav_bridge bridge.launch.py
```

---

## Known Issues

| Issue | Cause | Fix |
| :--- | :--- | :--- |
| Container exits immediately | `CMD ["/bin/bash"]` without TTY | Use `docker run -itd` or `compose up -d` |
| `livox_ros_driver2` build fails | Missing `package.xml` | `ln -s package_ROS2.xml package.xml` |
| PCL not found during build | Missing `libpcl-dev` in image | Add `libpcl-dev` + `ros-humble-pcl-ros` to `Dockerfile` |
| Intermittent node discovery | CycloneDDS random interface selection | Set `cyclonedds.xml` with `<NetworkInterfaceAddress>eth0</NetworkInterfaceAddress>` |

---

**Lead Researcher:** [ABBranca](https://github.com/ABBranca) — Politecnico di Milano, Master's Thesis in Autonomous Systems & Robotics
