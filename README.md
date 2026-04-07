# Unitree Go2 Autonomous Navigation Stack

[![ROS 2 Humble](https://img.shields.io/badge/ROS2-Humble-blue)](https://docs.ros.org/en/humble/)
[![Platform](https://img.shields.io/badge/Platform-ARM64%20(Orin)-green)](#)
[![Robot](https://img.shields.io/badge/Robot-Unitree%20Go2-orange)](https://www.unitree.com/go2)
[![License](https://img.shields.io/badge/License-Apache--2.0-yellow)](LICENSE)

Autonomous navigation framework for the **Unitree Go2** quadruped robot, integrating
LiDAR-Inertial SLAM (FAST-LIO2), Nav2, and a custom velocity bridge. Bachelor's thesis in
Mechatronics Engineering at **UNIMORE**.

| Requirement | Version / Value |
| :--- | :--- |
| ROS 2 | Humble Hawksbill |
| Platform | ARM64 — Jetson Orin (Expansion Dock) |
| Container | Docker + `docker compose` |
| LiDAR | Hesai XT16 |
| IMU | BMI088 (onboard Go2) |

---

## Quick Start

> **Prerequisites:** Docker with `buildx` + QEMU for ARM64, SSH access to `192.168.123.18`.

```bash
# Clone and initialize all submodules
git clone --recursive https://github.com/ABBranca/go2_ws.git
cd go2_ws

# Sync source to robot and start the container
./sync_to_dog.sh
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose up -d"

# Shell into the container and build
ssh unitree@192.168.123.18 "docker exec -it go2_navigation bash"
colcon build --symlink-install --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble
source install/setup.bash
```

For local development with VS Code Dev Containers, see [Development Workflow](#development-workflow).

---

## Architecture

The Hesai XT16 LiDAR streams point clouds over UDP. FAST-LIO2 fuses them with the onboard
BMI088 IMU via an Iterative Extended Kalman Filter (IEKF) and an incremental k-d tree (ikd-Tree),
publishing a continuous `map → odom → base_link` TF chain. Nav2 plans collision-free paths on
the resulting map using `SmacPlannerHybrid` + MPPI controller (tuned for quadruped kinematics).
`go2_nav_bridge` translates `cmd_vel` Twist messages into Unitree's proprietary `SportModeCmd`
protocol with velocity clamping and a watchdog safety layer.

```
Hesai XT16 (192.168.123.20, UDP:2368)
    └─ hesai_ros_driver_2  ──►  /lidar_points (PointCloud2)
                                          │
                                     FAST-LIO2  (IEKF + ikd-Tree)
                                          │
                         TF: map → odom → base_link → hesai_lidar
                                          │
                                     Nav2 Stack
                            (SmacPlannerHybrid + MPPI)
                                          │
                           go2_nav_bridge (cmd_vel → SportModeCmd)
                            mode=2 | ±1.0 m/s | ±1.0 rad/s
                            watchdog 200 ms | NaN/Inf sanitization
                                          │
                              MCU 192.168.123.161 (motion control)
```


---

## Packages

| Package | Type | Upstream | Role |
| :--- | :--- | :--- | :--- |
| `go2_nav_bridge` | local | — | `cmd_vel` → `SportModeCmd` translation; velocity clamping; watchdog; 21 unit tests |
| `unitree_ros2` | submodule | [unitreerobotics/unitree_ros2](https://github.com/unitreerobotics/unitree_ros2) | Unitree SDK message types (`unitree_go`, `unitree_api`) |
| `fast_lio_ros2` | submodule | [hku-mars/FAST_LIO](https://github.com/hku-mars/FAST_LIO) | Tightly-coupled LiDAR-IMU SLAM via IEKF + ikd-Tree |
| `hesai_ros_driver_2` | submodule | [HesaiTechnology/HesaiLidar_ROS2](https://github.com/HesaiTechnology/HesaiLidar_ROS2) | Hesai XT16 ROS 2 driver |
| `livox_ros_driver2` | submodule | [Livox-SDK/livox_ros_driver2](https://github.com/Livox-SDK/livox_ros_driver2) | Custom message types required by FAST-LIO2 (no Livox hardware needed) |

---

## Hardware & Network

### Network Topology

| Unit | IP | SSH | Role |
| :--- | :--- | :--- | :--- |
| Expansion Dock (Orin) | `192.168.123.18` | ✅ | Primary compute; Docker + ROS 2 stack |
| MCU (Motion Control) | `192.168.123.161` | ❌ blocked | Motors, low-level SDK, Wi-Fi adapter |
| Hesai XT16 LiDAR | `192.168.123.20` | — | UDP `2368` data, TCP `9347` PTC |
| Developer Laptop | `192.168.123.10` | — | Code editing, Rviz2 visualization |

> **Networking challenge:** The MCU does not bridge DDS traffic from Ethernet to its Wi-Fi
> adapter. Recommended solution: USB Wi-Fi dongle (Alfa AWUS036ACM, MediaTek MT7612U) directly
> on the Dock. Fallback: GL.iNet travel router on the Go2 Ethernet port for L2 bridging.

### LiDAR-IMU Extrinsics

| Parameter | Value | Source |
| :--- | :--- | :--- |
| `extrinsic_T` | `[0.171, 0.0, 0.0908]` m | Official Unitree documentation ⚠️ pending caliper verification |
| `extrinsic_R` | `I₃` (identity) | Official Unitree documentation |
| IMU noise (`gyr_cov`, `acc_cov`) | BMI088 datasheet estimates | ⚠️ pending 5-min static rosbag characterization |

Config file: [`src/fast_lio_ros2/config/hesai_xt16.yaml`](src/fast_lio_ros2/config/hesai_xt16.yaml)

---

## Installation

### Prerequisites

```bash
# Verify Docker buildx with QEMU (for ARM64 cross-compilation)
docker buildx ls   # must show linux/arm64 in supported platforms

# Verify SSH access
ssh unitree@192.168.123.18 "echo ok"
```

### Clone

```bash
git clone --recursive https://github.com/ABBranca/go2_ws.git
cd go2_ws
```

> `livox_ros_driver2/package.xml` is already committed to the repo — no manual symlink required.

### First-time Docker start on robot

```bash
# Sync source and start container
./sync_to_dog.sh
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose up --build -d"
```

### Build

```bash
docker exec -it go2_navigation bash
colcon build --symlink-install --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble
source install/setup.bash
```

Build a single package:

```bash
colcon build --symlink-install --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble \
  --packages-select go2_nav_bridge
```

> **Note:** `ament_cmake` packages symlink correctly; `ament_python` packages require a full
> rebuild when YAML/launch files change.

---

## Development Workflow

### Local (VS Code Dev Containers) — preferred for active development

Open the project root in VS Code and run `Dev Containers: Reopen in Container`.
The container includes all ROS 2 dependencies; use the **ROS 2 extension** for IntelliSense
and builds, or the integrated terminal:

```bash
colcon build --symlink-install --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble
source install/setup.bash
```

### Hardware Testing (sync → build → visualize)

```bash
# 1. Sync source to robot over Ethernet
./sync_to_dog.sh   # rsync src/ → unitree@192.168.123.18

# 2. Build inside the container on the robot
docker exec -it go2_navigation bash
colcon build --symlink-install --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble
source install/setup.bash

# 3. Visualize remotely on the laptop
source /opt/ros/humble/setup.bash
export ROS_DOMAIN_ID=1
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
```

### Final Deployment (ARM64 immutable image)

Requires `docker buildx` with QEMU configured for ARM64 cross-compilation
([Docker multi-platform docs](https://docs.docker.com/build/building/multi-platform/)).

```bash
# Build ARM64 image locally
docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load .

# Transfer to robot and start
docker save go2_nav_stack:latest | ssh -C unitree@192.168.123.18 'docker load'
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose up -d"
```

---

## Launch

Start nodes in this order:

```bash
# 1. Hesai XT16 LiDAR driver
ros2 launch hesai_ros_driver_2 start.py

# 2. Static TF: base_link → hesai_lidar (official Unitree extrinsics)
ros2 run tf2_ros static_transform_publisher \
  --x 0.171 --y 0.0 --z 0.0908 \
  --yaw 0 --pitch 0 --roll 0 \
  --frame-id base_link --child-frame-id hesai_lidar

# 3. FAST-LIO2 SLAM
ros2 launch fast_lio mapping.launch.py config_file:=hesai_xt16.yaml rviz:=true

# 4. Nav2 stack (lifecycle manager without map_server/amcl — FAST-LIO2 handles localization)
ros2 launch go2_nav_bridge navigation.launch.py

# 5. go2_nav_bridge (cmd_vel → SportModeCmd)
ros2 run go2_nav_bridge bridge_node
```

> **CycloneDDS:** Without explicit config, CycloneDDS picks a network interface randomly,
> causing intermittent node discovery failures. The container mounts
> [`docker/cyclonedds.xml`](docker/cyclonedds.xml) via `CYCLONEDDS_URI` with
> `<NetworkInterface name="eth0" />`. Verify with `ros2 node list` after launch.

---

## ROS Interfaces

### Topics

| Topic | Type | Direction | Publisher |
| :--- | :--- | :--- | :--- |
| `/lidar_points` | `sensor_msgs/PointCloud2` | pub | `hesai_ros_driver_2` |
| `/Odometry` | `nav_msgs/Odometry` | pub | `fast_lio_ros2` |
| `/tf`, `/tf_static` | `tf2_msgs/TFMessage` | pub | `fast_lio_ros2`, `static_transform_publisher` |
| `/cmd_vel` | `geometry_msgs/Twist` | sub | Nav2 → `go2_nav_bridge` |
| `/sportmodestate` | `unitree_go/SportModeState` | pub | MCU |

### `go2_nav_bridge` Node — Parameters

| Parameter | Type | Default | Range | Description |
| :--- | :--- | :--- | :--- | :--- |
| `linear_max` | `double` | `1.0` | `(0, ∞)` | Max linear velocity (m/s) applied symmetrically |
| `angular_max` | `double` | `1.0` | `(0, ∞)` | Max angular velocity (rad/s) applied symmetrically |
| `watchdog_timeout_ms` | `int` | `200` | `[1, ∞)` | Zero-velocity published after this silence interval |

> Parameters are **read-only** — validated at startup with fallback to defaults.
> Set via `ros2 param set` before node activation or in the launch file.

### TF Tree

```
map
 └─ odom          (FAST-LIO2)
     └─ base_link (FAST-LIO2)
         └─ hesai_lidar  (static: T=[0.171, 0, 0.0908] m, R=I₃)
```

---

## Research Context

**Thesis objectives:**
1. Deploy a full LiDAR-SLAM + Nav2 pipeline on a Unitree Go2 quadruped (ARM64).
2. Implement a velocity bridge that safely translates Nav2 output to proprietary motion commands.

**Roadmap:**

| Milestone | Status |
| :--- | :--- |
| Hesai XT16 ROS 2 driver integration | ✅ |
| FAST-LIO2 config for XT16 (hesai_xt16.yaml) | ✅ |
| `go2_nav_bridge` (cmd_vel → SportModeCmd) | ✅ |
| Docker ARM64 image build & transfer | ⏳ Pending |
| LiDAR-IMU extrinsics field verification | ⚠️ Pending |
| IMU noise characterization (static rosbag) | ⚠️ Pending |
| End-to-end test: LiDAR → SLAM → Nav2 → bridge → robot motion | ⏳ Pending |
| 2D occupancy grid from FAST-LIO2 point cloud (octomap_server) | ⏳ Pending |

**Related work:** SayCan (Ahn et al., 2022), Code as Policies (Liang et al., 2023),
ProgPrompt (Singh et al., 2023), VoxPoser (Huang et al., 2023).

---

## Known Issues

| Issue | Status | Notes |
| :--- | :--- | :--- |
| Container exits immediately (Exit Code 0) | ✅ Fixed | `docker-compose.yml` sets `stdin_open: true` and `tty: true` |
| `livox_ros_driver2` build fails | ✅ Fixed | `package.xml` committed; colcon passes `-DROS_EDITION=ROS2 -DHUMBLE_ROS=humble` |
| PCL not found during build | ✅ Fixed | `libpcl-dev` and `ros-humble-pcl-ros` added to `Dockerfile` |
| `rmw_cyclonedds_cpp` not installed | ✅ Fixed | `ros-humble-rmw-cyclonedds-cpp` added to `Dockerfile` |
| CycloneDDS random interface selection | ✅ Fixed | `docker/cyclonedds.xml` with `eth0` mounted via `CYCLONEDDS_URI` |
| CycloneDDS default socket buffer (212 KB) drops PointCloud2 bursts | ✅ Fixed | 4 MB `SocketReceiveBufferSize` in `cyclonedds.xml`; host requires `sudo sysctl -w net.core.rmem_max=4194304` |
| `docker-compose` `privileged: true` (excessive Linux capabilities) | ✅ Fixed | Replaced with `cap_add: [NET_ADMIN, SYS_NICE]` |
| `devcontainer/ros_entrypoint.sh` crashes before first `colcon build` | ✅ Fixed | Guarded `source install/setup.bash` with `[ -f ]` check; `.bashrc` appends are idempotent |
| `.vscode/settings.json` hardcoded machine-specific `clangd.path` | ✅ Fixed | Replaced with `"clangd"` (resolved from PATH) |
| MCU does not bridge DDS traffic to Wi-Fi | ⏳ Open | USB Wi-Fi dongle (Alfa AWUS036ACM) on Dock or GL.iNet travel router |
| `unitree_ros2` deprecation warnings on build | ℹ️ Harmless | `ament_export_interfaces` / `rosidl_target_interfaces` target Foxy; ignore |

---

## Citation

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
