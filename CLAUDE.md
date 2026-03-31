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
colcon build --symlink-install   # NOTE: instant only for ament_cmake packages; ament_python requires rebuild on YAML/launch changes
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

**Alternative networking strategies:**
- **WebRTC/DDS Hybrid:** Use WebRTC for LiDAR/video streams and DDS for control commands — gold standard for wireless telemetry.
- **IP Forwarding on Dock:** If a Wi-Fi dongle is added to the Dock, enable NAT to reach the MCU:
  ```bash
  sudo sysctl -w net.ipv4.ip_forward=1
  sudo iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
  ```
- **Travel Router:** External router (e.g. GL.iNet) connected to the Go2 Ethernet port for transparent L2 bridging.
- **Recommended dongle:** Alfa Network AWUS036ACM (MediaTek MT7612U) — native Linux driver, ARM64 compatible.

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
  - Mapping: `mode=2` (VelocityMove), `velocity[0]=linear.x`, `velocity[1]=linear.y`, `yaw_speed=angular.z`
  - Clamp values: linear ±1.0 m/s, angular ±1.0 rad/s. QoS must match Nav2 (`RELIABLE`, depth 10).
  - Package type must be `ament_cmake` (not `ament_python`) so YAML/launch files are symlinked correctly.
- **FAST-LIO2 config for Hesai XT16:** No native config exists — extrinsic calibration and IMU noise parameters must be adapted from an existing YAML in `src/fast_lio_ros2/config/`.
- **unitree_ros2 ROS version:** The `setup.sh` scripts in the submodule target Foxy; ignore them and source the Humble workspace. Deprecation warnings during build (`ament_export_interfaces`, `rosidl_target_interfaces`) are harmless.
- **Hesai launch file:** `start.launch` is ROS 1 XML — a ROS 2 Python equivalent is needed.
- **livox_ros_driver2:** A `package.xml` was manually added to make it compile under ament_cmake; the upstream repo does not include one.
- **README.md mandate:** Must stay in English and always include links to submodules with short descriptions.
- **CycloneDDS in Docker:** Without explicit config, CycloneDDS picks a network interface randomly — causes intermittent node discovery failures. Always provide a `cyclonedds.xml` with `<NetworkInterface name="eth0" />` and set `CYCLONEDDS_URI` in the container.
- **TF static transform:** Publish `base_link → lidar_link` via `static_transform_publisher` or `robot_state_publisher` (URDF). In Humble, static TFs must be on `/tf_static` only — do not publish them periodically on `/tf`.
- **Nav2 for quadruped:** Do not use default NavFn + DWB (differential-drive only). Use `SmacPlannerHybrid` + MPPI controller. Key param: `minimum_turning_radius` (0.3–0.5 m for Go2).
- **Nav2 lifecycle manager:** With FAST-LIO2 active, omit `map_server` and `amcl` from `node_names` — FAST-LIO2 already publishes `map → odom → base_link`. Only needed: `[controller_server, planner_server, behavior_server, bt_navigator]`.

## Development Conventions

- **Bridge node:** All custom navigation command translation logic must reside in `src/go2_nav_bridge`.
- **Message types:** Always prefer high-level `SportModeCmd` over `LowCmd` — leverages the robot's onboard stability controllers.
- **TF Tree:**
  - `map` → `odom` (provided by FAST-LIO2)
  - `odom` → `base_link` (provided by FAST-LIO2)
  - `base_link` → `lidar_link` (static transform, manually calibrated)

## FAST-LIO2 Reference

Key concepts for configuration and debugging:

- **ikd-Tree:** Incremental k-d tree — supports efficient point insertion/deletion at high LiDAR frequencies (>100 Hz).
- **Direct odometry:** Registers raw points directly to the map (no feature extraction) — more robust in low-feature environments.
- **Tightly-coupled fusion:** LiDAR + IMU via Iterative Extended Kalman Filter (IEKF).
- **Sensor support:** Rotating LiDARs (Hesai, Velodyne, Ouster) and solid-state (Livox).
- **Config to adapt:** Copy an existing YAML from `src/fast_lio_ros2/config/` and tune `extrinsic_T`, `extrinsic_R`, and IMU noise params (`gyr_cov`, `acc_cov`, `b_gyr_cov`, `b_acc_cov`).

## Generative AI Integration (MCP)

The high-level logic of the Go2 will be enhanced using the **Model Context Protocol (MCP)** to turn the robot into an autonomous agent.

### Control Layer Architecture (3T)

The system must separate concerns across three frequency domains — the LLM must never operate in the reactive or executive loops:

```
┌──────────────────────────────────────────────────────┐
│  DELIBERATIVE LAYER  (~0.1 Hz)                       │
│  LLM (external) → MCP Client → MCP Server (Orin)    │
│  Task planning, semantic grounding, goal generation  │
├──────────────────────────────────────────────────────┤
│  REACTIVE LAYER  (~20 Hz)                            │
│  Nav2 (costmap, planner, MPPI controller)            │
│  Obstacle avoidance, path following                  │
├──────────────────────────────────────────────────────┤
│  EXECUTIVE LAYER  (~50 Hz)                           │
│  go2_nav_bridge (cmd_vel → SportModeCmd)             │
│  Velocity clamping, watchdog, MCU interface          │
└──────────────────────────────────────────────────────┘
```

The LLM issues Nav2 Action goals; it has **no direct authority** over `cmd_vel`.

### MCP Server (on Orin Dock)

- Runs as a ROS 2 node inside the `go2_navigation` container
- Exposes tools via HTTP/WebSocket (MCP protocol)
- Requires Wi-Fi dongle on Dock (see Network Topology); fallback: travel router or offline mode

### mcp_watchdog Node (mandatory safety component)

A dedicated ROS 2 node, running on-robot and independent of the Wi-Fi link, must:
1. Monitor a heartbeat from the MCP client (expected period: configurable, suggested 5 s)
2. Cancel any active Nav2 goal via Action Client if heartbeat is lost for > N seconds
3. Command `set_stance("stand_still")` or `set_stance("sit")` as safe-stop

This node is a prerequisite for any MCP deployment on a mobile platform.

### Tool API Specification

Each tool must have explicit signatures, pre/post-conditions, and failure modes:

```python
navigate_to(
    target: Pose2D | str,          # Pose2D or named waypoint from spatial map
    max_velocity: float = 0.5,     # m/s, overrides Nav2 default
    timeout_sec: float = 120.0,
    on_failure: Literal["abort", "retry", "return_home"] = "abort"
) -> NavigationResult              # {success, final_pose, distance_m, elapsed_sec, error}

cancel_navigation() -> bool        # Cancels active Nav2 goal; returns True if goal was active

get_current_pose() -> Pose2D       # Robot pose in map frame (from FAST-LIO2 TF)

get_battery_status() -> BatteryStatus     # Voltage, SoC%, estimated remaining time
get_slam_state() -> SlamState             # Tracking state, map size, last update timestamp
get_sensor_health() -> SensorHealth       # LiDAR, IMU status; split from telemetry blob

set_stance(
    mode: Literal["sit", "stand", "stand_still", "balance_stand"]
) -> bool                          # Via SportModeCmd; note: "pose" is ambiguous — use "stance"

visual_inspection(
    camera: Literal["front_left", "front_right"] = "front_left",
    analysis_prompt: str = "Describe the scene."
) -> InspectionResult              # {image_base64, vlm_response, timestamp}
# NOTE: VLM location (on-Orin vs. cloud) and Orin memory budget must be decided before implementation.
# FAST-LIO2 + Nav2 + VLM concurrent load on Orin (8 GB shared) requires profiling.

wait(seconds: float) -> None       # Temporal primitive for multi-step task sequencing
```

**Spatial grounding for `navigate_to`:** Named waypoints must be defined in a static YAML map
(`config/waypoints.yaml`) populated during an initial mapping session. Dynamic semantic mapping
is out of scope for the thesis.

### Connectivity & Fallback Plan

Wi-Fi via Alfa AWUS036ACM dongle on the Dock is the primary path. If the driver is incompatible
with the Orin kernel (ARM64), fallback options in priority order:
1. **Travel router** (GL.iNet) on the Go2 Ethernet port — transparent L2 bridging, no driver issues
2. **Tethered mode** — laptop connected via Ethernet to the Dock during demonstrations
3. **Local LLM** — small model (e.g. Qwen2-0.5B) on Orin; severely constrained but fully offline

### Related Work

Any thesis chapter on this architecture must position against: SayCan (Ahn et al., 2022),
Code as Policies (Liang et al., 2023), ProgPrompt (Singh et al., 2023), VoxPoser (Huang et al., 2023).
Key differentiator to argue: first application of MCP (as opposed to ad-hoc tool-use APIs)
to an embodied quadruped platform with a full SLAM + Nav2 stack.

### Implementation Milestone Ordering

**Do NOT implement the MCP layer until the following milestone is verified end-to-end:**
> LiDAR → FAST-LIO2 → Nav2 → go2_nav_bridge → physical robot motion

Only after this baseline is demonstrated should the MCP Server be added as an incremental layer.

## External Resources & Community Solutions

### Key GitHub Repositories
- **[abizovnuralem/go2_ros2_sdk](https://github.com/abizovnuralem/go2_ros2_sdk):** Supports WebRTC for fluid telemetry over Wi-Fi. Useful if standard DDS proves too heavy/unstable.
- **[unitree_go2_nav](https://github.com/Sayantani-Bhattacharya/unitree_go2_nav):** Autonomous navigation example with Nav2 and SLAM RTAB-Map on Go2.
- **[Unitree-Go2-Robot/go2_robot](https://github.com/Unitree-Go2-Robot/go2_robot):** Standard integration for `cmd_vel` and complete URDF.

## Upcoming Tasks & Thesis Analysis

A thesis from a previous student on the Unitree Go2 + LiDAR combination will be provided. Analysis objectives:
- **Networking:** Identify how Wi-Fi connectivity and remote telemetry were solved (DDS, Bridge, or dedicated hardware).
- **Calibration:** Extract extrinsic matrices (TF) between the Go2 body and the Hesai LiDAR.
- **FAST-LIO2 Tuning:** Recover IMU noise parameters and XT16-specific configurations.

## TODO / Roadmap

- [x] Integrate ROS 2 driver for Hesai XT16 LiDAR
- [x] Connect robot to laboratory Wi-Fi (ARSCONTROL)
- [x] Initialize and update all Git submodules
- [x] Establish SSH access to Expansion Dock (`192.168.123.18`)
- [x] Diagnose MCU networking (DDS traffic not bridged to Wi-Fi)
- [ ] **[Hardware/Calibration]** Measure Hesai XT16 extrinsic matrices (CAD/Manual) and verify Wi-Fi dongle driver compatibility
- [ ] **[Hardware/Calibration]** Characterize IMU noise (BMI088) via 5-min static rosbag recording
- [ ] Create `cyclonedds.xml` with explicit `<NetworkInterface>` and mount it in Docker Compose via `CYCLONEDDS_URI`
- [ ] Implement `go2_nav_bridge`: `cmd_vel` → `SportModeCmd` translation (mode=2, see Key Implementation Notes)
- [ ] Verify `go2_nav_bridge` is `ament_cmake` (not `ament_python`) for correct `--symlink-install` behavior on YAML/launch files
- [ ] Publish `base_link → lidar_link` static TF via `static_transform_publisher` in the bridge launch file
- [ ] Configure FAST-LIO2 YAML for Hesai XT16 (extrinsics + IMU noise parameters)
- [ ] **[SLAM Extension]** Integrate `octomap_server` or similar to provide 2D Occupancy Grid from FAST-LIO2 point cloud for Nav2
- [ ] Configure Nav2 with `SmacPlannerHybrid` planner + MPPI controller (replace default NavFn + DWB)
- [ ] Configure Nav2 lifecycle manager `node_names` — exclude `map_server` and `amcl` (FAST-LIO2 handles localization)
- [ ] Resolve wireless telemetry (Wi-Fi dongle on Dock or DDS Discovery Server)
- [ ] **[Baseline Validation]** Verify end-to-end navigation: LiDAR → FAST-LIO2 → Nav2 → bridge → physical motion
- [ ] **[Baseline Validation]** Conduct at least 3 trials on a measured path to establish quantitative RMSE/FLE baseline
- [ ] **[MCP prerequisite]** Define spatial grounding map (`config/waypoints.yaml`) with named waypoints
- [ ] Implement `mcp_watchdog` node: heartbeat monitor + Nav2 goal cancellation + safe-stop on link loss
- [ ] Implement MCP Server on Expansion Dock with formal tool API (see tool signatures in MCP section)
- [ ] Profile Orin memory budget: FAST-LIO2 + Nav2 + VLM concurrent load (8 GB shared)
- [ ] Build and deploy final ARM64 Docker image (`docker save/load`)
