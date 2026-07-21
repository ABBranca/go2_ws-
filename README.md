# Unitree Go2 Autonomous Navigation Stack

[![ROS 2 Humble](https://img.shields.io/badge/ROS2-Humble-blue)](https://docs.ros.org/en/humble/)
[![Platform](https://img.shields.io/badge/Platform-ARM64%20(Orin)-green)](#)
[![Robot](https://img.shields.io/badge/Robot-Unitree%20Go2-orange)](https://www.unitree.com/go2)
[![License](https://img.shields.io/badge/License-Apache--2.0-yellow)](LICENSE)

Autonomous navigation framework for the **Unitree Go2** quadruped robot, integrating
2D LiDAR SLAM (slam_toolbox), Nav2, and a custom velocity bridge. Bachelor's thesis in
Mechatronics Engineering at **UNIMORE**.

| Requirement | Version / Value |
| :--- | :--- |
| ROS 2 | Humble Hawksbill |
| Platform | ARM64 — Jetson Orin (Expansion Dock) |
| Container | Docker + `docker compose` |
| LiDAR | Hesai XT16 |
| IMU | Go2 onboard (`/utlidar/imu`) |

---

## Quick Start

> **Prerequisites:** Docker with `buildx` + QEMU for ARM64, SSH access to `192.168.123.18`.
> Submodules must be initialized on the developer laptop before syncing.

```bash
# Clone and initialize all submodules
git clone --recursive https://github.com/ABBranca/go2_ws.git
cd go2_ws

# Sync source to robot, build the Docker image, and start the full stack
./sync_to_dog.sh
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose up --build -d"
```

The container builds the ROS 2 workspace during `docker compose build` and automatically
launches the full navigation stack on `docker compose up`. No further manual steps are needed.

To visualize on the developer laptop:

```bash
source /opt/ros/humble/setup.bash
export ROS_DOMAIN_ID=0
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
```

For local development with VS Code Dev Containers, see [Development Workflow](#development-workflow).

---

## Architecture

The Hesai XT16 LiDAR streams point clouds over UDP. Because a horizontally-mounted XT16
(16 beams, ±15° vertical FOV) cannot observe vertical translation, the stack uses **2D SLAM**:
`pointcloud_to_laserscan` flattens the cloud into a `/scan` inside an IMU-levelled frame, and
`slam_toolbox` builds a 2D occupancy grid while publishing the `map → odom` correction. Planar,
Z-free odometry is sourced from the Go2's onboard leg kinematics (`/utlidar/robot_odom`) via
`odom_tf_broadcaster`, which also removes gait-induced tilt using the body IMU (`/utlidar/imu`).
Nav2 plans collision-free paths on the resulting map using `SmacPlannerHybrid` + MPPI controller
(tuned for quadruped kinematics). `go2_nav_bridge` translates `cmd_vel` Twist messages into
Unitree's proprietary `SportModeCmd` protocol with velocity clamping and a watchdog safety layer.

```
Hesai XT16 (192.168.123.20, UDP:2368)
    └─ hesai_ros_driver_2  ──►  /lidar_points (PointCloud2)
                                          │
                             pointcloud_to_laserscan  ──►  /scan (LaserScan)
                                          │
                                     slam_toolbox  (2D SLAM)
                                          │
                        TF: map → odom   +   /map (OccupancyGrid)
                                          │
  Go2 leg odom /utlidar/robot_odom ─► odom_tf_broadcaster ─► TF: odom → base_link
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
| `allan_variance_ros2` | submodule | [Autoliv-Research/allan_variance_ros2](https://github.com/Autoliv-Research/allan_variance_ros2) | IMU characterization (Angle Random Walk, Bias Instability) via Allan Variance |
| `hesai_ros_driver_2` | submodule | [HesaiTechnology/HesaiLidar_ROS2](https://github.com/HesaiTechnology/HesaiLidar_ROS2) | Hesai XT16 ROS 2 driver |

> **SLAM & scan conversion** are provided by upstream apt packages (`ros-humble-slam-toolbox`,
> `ros-humble-pointcloud-to-laserscan`), installed by the `Dockerfile` — not workspace packages.
> The previous 3D pipeline (`fast_lio_ros2`, `livox_ros_driver2`, `octomap_server`) was retired
> in favour of 2D SLAM; it remains recoverable from git history.

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

### LiDAR Extrinsics

| Parameter | Value | Source |
| :--- | :--- | :--- |
| `T` (base_link → hesai_lidar) | `[0.171, 0.0, 0.0908]` m | Official Unitree documentation ⚠️ pending caliper verification |
| `R` | `I₃` (identity) | Official Unitree documentation |
| IMU noise (`gyr_cov`, `acc_cov`) | Measured values (Allan Variance) | ✅ Analyzed via `allan_variance_ros2` |

The extrinsic is applied as a static transform (`base_link → hesai_lidar`) in
[`src/go2_nav_bridge/launch/bringup.launch.py`](src/go2_nav_bridge/launch/bringup.launch.py).

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

### First-time Docker start on robot

```bash
# Sync source, build the image, and start the full stack
./sync_to_dog.sh
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose up --build -d"
```

The workspace is compiled inside the Docker image at build time. No manual
`colcon build` is needed — `docker compose up -d` launches the full navigation
stack automatically.

---

## Development Workflow

### Local (VS Code Dev Containers) — preferred for active development

Open the project root in VS Code and run `Dev Containers: Reopen in Container`.
Use the **ROS 2 extension** for IntelliSense and builds, or the integrated terminal:

```bash
colcon build --symlink-install --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble
source install/setup.bash
```

### Hardware Testing (edit → sync → rebuild image → redeploy)

```bash
# 1. Sync updated source to robot
./sync_to_dog.sh

# 2. Rebuild the Docker image on the robot and restart the stack
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose up --build -d"

# 3. Visualize remotely on the laptop
source /opt/ros/humble/setup.bash
export ROS_DOMAIN_ID=0
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
```

### Interactive Development Shell (dev profile)

To get a bash shell inside the container with the source mounted as a volume
(for rapid iteration without rebuilding the image):

```bash
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose --profile dev up -d"
ssh unitree@192.168.123.18 "docker exec -it go2_navigation_dev bash"
# Inside the container:
colcon build --symlink-install --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble
source install/setup.bash
ros2 launch go2_nav_bridge bringup.launch.py
```

### Final Deployment (ARM64 immutable image)

Requires `docker buildx` with QEMU configured for ARM64 cross-compilation
([Docker multi-platform docs](https://docs.docker.com/build/building/multi-platform/)).

```bash
# Build ARM64 image locally and transfer to robot
docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load docker/
docker save go2_nav_stack:latest | ssh -C unitree@192.168.123.18 'docker load'
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose up -d"
```

---

---

## ROS Interfaces

### Topics

| Topic | Type | Direction | Publisher |
| :--- | :--- | :--- | :--- |
| `/lidar_points` | `sensor_msgs/PointCloud2` | pub | `hesai_ros_driver_2` |
| `/scan` | `sensor_msgs/LaserScan` | pub | `pointcloud_to_laserscan` |
| `/map` | `nav_msgs/OccupancyGrid` | pub | `slam_toolbox` |
| `/tf`, `/tf_static` | `tf2_msgs/TFMessage` | pub | `slam_toolbox` (map→odom), `odom_tf_broadcaster` (odom→base_link), `static_transform_publisher` |
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
 └─ odom          (slam_toolbox)
     └─ base_link (odom_tf_broadcaster — Go2 leg odometry)
         └─ hesai_lidar  (static: T=[0.171, 0, 0.0908] m, R=I₃)
```

---

## Research Context

**Thesis objectives:**
1. Deploy a full 2D LiDAR-SLAM + Nav2 pipeline on a Unitree Go2 quadruped (ARM64).
2. Implement a velocity bridge that safely translates Nav2 output to proprietary motion commands.

**Roadmap:**

| Milestone | Status |
| :--- | :--- |
| Hesai XT16 ROS 2 driver integration | ✅ |
| 2D SLAM config for XT16 (slam_toolbox_2d.yaml) | ✅ |
| `go2_nav_bridge` (cmd_vel → SportModeCmd) | ✅ |
| Docker ARM64 image build & transfer | ⏳ Pending |
| LiDAR-IMU extrinsics field verification | ⚠️ Pending |
| IMU noise characterization (Allan Variance) | ✅ Completed |
| End-to-end test: LiDAR → SLAM → Nav2 → bridge → robot motion | ⏳ Pending |
| 2D occupancy grid via slam_toolbox (from projected `/scan`) | ✅ |

**Related work:** SayCan (Ahn et al., 2022), Code as Policies (Liang et al., 2023),
ProgPrompt (Singh et al., 2023), VoxPoser (Huang et al., 2023).

---

## Known Issues

| Issue | Status | Notes |
| :--- | :--- | :--- |
| Container exits immediately (Exit Code 0) | ✅ Fixed | `docker-compose.yml` sets `stdin_open: true` and `tty: true` |
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
