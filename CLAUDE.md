# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Development Workflow

The standard cycle is: edit locally → sync to robot → build in container → visualize on laptop.

**1. Sync code to robot (Ethernet):**
```bash
./sync_to_dog.sh   # rsync src/ → unitree@192.168.123.18
```

**2. Build inside the Docker container on the robot:**
```bash
docker exec -it go2_navigation bash
colcon build --symlink-install   # Python/YAML changes are instant, no rebuild needed
source install/setup.bash
```

**3. Visualize remotely (on laptop):**
```bash
source /opt/ros/humble/setup.bash
export ROS_DOMAIN_ID=1
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
```

## Build & Deploy

**Initial Docker start (on robot):**
```bash
cd docker && docker compose up --build -d
```

**Build a single package:**
```bash
colcon build --symlink-install --packages-select go2_nav_bridge
```

**Final immutable image (ARM64, when development is complete):**
```bash
docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load .
docker save go2_nav_stack:latest | ssh -C unitree@192.168.123.18 'docker load'
```

**After cloning — initialize submodules:**
```bash
git submodule update --init --recursive
```

## Launch

```bash
# Hesai XT16 LiDAR driver
ros2 launch hesai_ros_driver_2 start.py  # ROS 2 conversion of start.launch

# FAST-LIO2 SLAM
ros2 launch fast_lio mapping.launch.py config_file:=mid360.yaml rviz:=true
```

## Architecture

```
Hesai XT16 (192.168.1.201:2368 UDP)
    └─ hesai_ros_driver_2  →  /lidar_points (PointCloud2)
                                        │
                                   FAST-LIO2
                                        │
                       TF: map → odom → base_link → lidar_link
                                        │
                                   Nav2 Stack
                                        │
                              go2_nav_bridge (cmd_vel → SportModeCmd)
                                        │
                              MCU 192.168.123.161 (motion control)
```

**ROS 2 environment:** Humble inside Docker (`go2_navigation` container), CycloneDDS, `ROS_DOMAIN_ID=1`.

## Network Topology

| Unit | IP | SSH | Role |
|---|---|---|---|
| MCU (Motion Control) | `192.168.123.161` | **blocked** | Motors, low-level SDK, Wi-Fi adapter |
| Expansion Dock (Orin) | `192.168.123.18` | enabled | Runs Docker + full ROS 2 stack |
| Developer Laptop | `192.168.123.10` | — | Code editing, Rviz2 visualization |
| Hesai XT16 LiDAR | `192.168.1.201` | — | UDP `2368` data, TCP `9347` PTC |

**Networking challenge:** The MCU does not bridge ROS 2 DDS traffic from internal Ethernet (`192.168.123.x`) to its Wi-Fi adapter. SSH on MCU is blocked, so IP forwarding/NAT cannot be configured there. Proposed solution: USB Wi-Fi dongle (e.g. Alfa AWUS036ACM) directly on the Dock, giving it a direct presence on the lab Wi-Fi (`ARSCONTROL`).

## Packages

| Package | Type | Role |
|---|---|---|
| `go2_nav_bridge` | local | Translates Nav2 `cmd_vel` → Unitree `SportModeCmd` — **currently a stub** |
| `unitree_ros2` | submodule | Unitree SDK + message types (`unitree_go`, `unitree_api`) |
| `fast_lio_ros2` | submodule (ROS2 branch) | Tightly-coupled LiDAR-IMU SLAM via IEKF + ikd-Tree |
| `hesai_ros_driver_2` | submodule | Official Hesai XT16 driver |
| `livox_ros_driver2` | submodule | Provides custom message types required by FAST-LIO2 |

## Key Implementation Notes

- **Bridge node:** Use `SportModeCmd` (high-level) rather than `LowCmd` (joint-level) for locomotion. The `Twist → SportModeCmd` mapping in `go2_nav_bridge` is not yet implemented.
- **FAST-LIO2 config for Hesai XT16:** No native config exists — extrinsic calibration and IMU noise parameters must be adapted from an existing YAML in `src/fast_lio_ros2/config/`.
- **unitree_ros2 ROS version:** The `setup.sh` scripts in the submodule target Foxy; ignore them and source the Humble workspace.
- **Hesai launch file:** `start.launch` is ROS 1 XML — a ROS 2 Python equivalent is needed.
- **livox_ros_driver2:** A `package.xml` was manually added to make it compile under ament_cmake; the upstream repo does not include one.
- **README.md mandate:** Must stay in English and always include links to submodules with short descriptions (per GEMINI.md).
