# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Development Workflow

Three workflows depending on context:

### Production deployment (single command)
The standard workflow for deploying and running the full navigation stack on the robot.
```bash
./sync_to_dog.sh
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose up --build -d"
```
The Dockerfile is a multi-stage build: stage 1 (builder) compiles the workspace with `colcon build`; stage 2 (runtime) copies only `install/` into a lean image. The `CMD` launches `bringup.launch.py` which orchestrates the full stack (Hesai driver → static TF → FAST-LIO2 → Nav2 → go2_nav_bridge). **No manual `colcon build` or `ros2 launch` needed.**

### Interactive development shell (dev profile)
For rapid iteration without rebuilding the Docker image:
```bash
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose --profile dev up -d"
ssh unitree@192.168.123.18 "docker exec -it go2_navigation_dev bash"
# Inside the container:
colcon build --symlink-install --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble
source install/setup.bash
ros2 launch go2_nav_bridge bringup.launch.py
```

### Local development (VS Code Dev Containers)
The preferred IDE environment for active development. Open the project root in VS Code and use the `Dev Containers: Reopen in Container` command. The container is pre-configured with all ROS 2 dependencies and the **ROS 2 extension** provides IntelliSense, debugging, and build management. Build via the extension sidebar or the integrated terminal (`colcon build --symlink-install`).

## Build & Deploy

**Start the full stack on robot (builds image if needed):**
```bash
cd docker && docker compose up --build -d
```

**Build a single package (inside dev container):**
```bash
colcon build --symlink-install --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble --packages-select go2_nav_bridge
```

**Final immutable image (ARM64 cross-compiled):**
```bash
docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load docker/
docker save go2_nav_stack:latest | ssh -C unitree@192.168.123.18 'docker load'
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose up -d"
```

**After cloning — initialize submodules:**
```bash
git submodule update --init --recursive
```

## Launch

The full stack is launched automatically via `docker compose up -d`. The master launch file is `go2_nav_bridge/launch/bringup.launch.py` which orchestrates:
1. `static_transform_publisher` — TF `base_link → hesai_lidar`
2. `hesai_ros_driver` — Hesai XT16 LiDAR driver (`start.py`)
3. `fast_lio` — FAST-LIO2 SLAM (`mapping.launch.py`, config: `hesai_xt16.yaml`, rviz disabled)
4. `nav2_bringup` — Nav2 stack via `nav2.launch.py` (SmacPlannerHybrid + MPPI, no map_server/amcl)
5. `go2_nav_bridge` — `cmd_vel → SportModeCmd` bridge (respawn enabled)

For manual launch in the dev container:
```bash
ros2 launch go2_nav_bridge bringup.launch.py
```

## Architecture

```
Hesai XT16 (192.168.123.20:2368 UDP)
    └─ hesai_ros_driver_2  →  /lidar_points (PointCloud2)
                                        │
                                   FAST-LIO2
                                        │
                       TF: map → odom → base_link → hesai_lidar
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
| Hesai XT16 LiDAR | `192.168.123.20` | — | UDP `2368` data, TCP `9347` PTC |

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
| `go2_nav_bridge` | local | Translates Nav2 `cmd_vel` → Unitree `SportModeCmd` (Apache-2.0, hardened build) |
| `unitree_ros2` | submodule | Unitree SDK + message types (`unitree_go`, `unitree_api`) |
| `fast_lio_ros2` | submodule (ROS2 branch) | Tightly-coupled LiDAR-IMU SLAM via IEKF + ikd-Tree |
| `hesai_ros_driver_2` | submodule | Official Hesai XT16 driver |
| `livox_ros_driver2` | submodule | Provides custom message types required by FAST-LIO2 |

## Key Implementation Notes

- **Bridge node (`go2_nav_bridge`):** Fully implemented `cmd_vel` → `SportModeCmd` translation with safety hardening.
  - Mapping: `mode=2` (VelocityMove), `velocity[0]=linear.x`, `velocity[1]=linear.y`, `yaw_speed=angular.z`
  - Clamp values: linear ±1.0 m/s, angular ±1.0 rad/s (configurable via read-only parameters). QoS: `RELIABLE`, depth 10.
  - **Safety features:** Watchdog timer (default 200 ms) publishes zero velocity on `cmd_vel` timeout; NaN/Inf input sanitization; symmetric clamping with `std::abs(limit)`; graceful shutdown via `rclcpp::on_shutdown` with 50 ms DDS flush delay.
  - **Parameters (read-only):** `linear_max` (m/s), `angular_max` (rad/s), `watchdog_timeout_ms` — validated at startup with fallback to defaults.
  - **Thread safety:** `std::mutex` guards time state; `std::atomic<bool>` for flags; consistent lock ordering in `publish_stop_command()`.
  - **Build hardening:** `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion`, `-D_FORTIFY_SOURCE=2` (non-Debug), RELRO linking.
  - **Tests:** 21 GTest unit tests for `bridge_utils` (edge cases: NaN, Inf, negative limits, boundary conditions) + full `ament_lint_auto` suite (copyright, cpplint, uncrustify, cppcheck, lint_cmake, xmllint).
  - **License:** Apache-2.0 (chosen for `ament_copyright` compatibility — avoids requiring full BSD-3 license text in each file header).
  - Package type: `ament_cmake` (not `ament_python`) so YAML/launch files are symlinked correctly.
- **FAST-LIO2 config for Hesai XT16:** Config file available at `src/fast_lio_ros2/config/hesai_xt16.yaml`. Extrinsics sourced from official Unitree documentation: `extrinsic_T: [0.171, 0.0, 0.0908]` (m), `extrinsic_R: I₃` (no relative rotation between IMU body and LiDAR frames). IMU noise params (BMI088 datasheet estimates) still require characterization via 5-min static rosbag.
- **unitree_ros2 ROS version:** The `setup.sh` scripts in the submodule target Foxy; ignore them and source the Humble workspace. Deprecation warnings during build (`ament_export_interfaces`, `rosidl_target_interfaces`) are harmless.
- **Hesai launch file:** `start.launch` is ROS 1 XML — a ROS 2 Python equivalent is needed.
- **livox_ros_driver2:** A `package.xml` was manually added to make it compile under ament_cmake; the upstream repo does not include one.
- **README.md mandate:** Must stay in English and always include links to submodules with short descriptions.
- **CycloneDDS in Docker:** Without explicit config, CycloneDDS picks a network interface randomly — causes intermittent node discovery failures. Always provide a `cyclonedds.xml` with `<NetworkInterface name="eth0" />` and set `CYCLONEDDS_URI` in the container.
- **TF static transform:** Publish `base_link → hesai_lidar` — frame name must match `ros_frame_id: hesai_lidar` in `hesai_ros_driver_2/config/config.yaml`. Use named-argument syntax in Humble (positional is deprecated): `ros2 run tf2_ros static_transform_publisher --x 0.171 --y 0.0 --z 0.0908 --yaw 0 --pitch 0 --roll 0 --frame-id base_link --child-frame-id hesai_lidar`. Static TFs must be on `/tf_static` only — do not publish them periodically on `/tf`.
- **Nav2 for quadruped:** Do not use default NavFn + DWB (differential-drive only). Use `SmacPlannerHybrid` + MPPI controller. Key param: `minimum_turning_radius` (0.3–0.5 m for Go2).
- **Nav2 lifecycle manager:** With FAST-LIO2 active, omit `map_server` and `amcl` from `node_names` — FAST-LIO2 already publishes `map → odom → base_link`. Only needed: `[controller_server, planner_server, behavior_server, bt_navigator]`.

## Development Conventions

- **Bridge node:** All custom navigation command translation logic must reside in `src/go2_nav_bridge`.
- **Message types:** Always prefer high-level `SportModeCmd` over `LowCmd` — leverages the robot's onboard stability controllers.
- **TF Tree:**
  - `map` → `odom` (provided by FAST-LIO2)
  - `odom` → `base_link` (provided by FAST-LIO2)
  - `base_link` → `hesai_lidar` (static transform — official Unitree extrinsics: T=[0.171, 0, 0.0908] m, R=I₃)
- **Claude Code subagents:** Parallel agents do not inherit the parent session's Edit/Write permissions. Use agents for read-only research/analysis; apply all file writes in the main session.

## FAST-LIO2 Reference

Key concepts for configuration and debugging:

- **ikd-Tree:** Incremental k-d tree — supports efficient point insertion/deletion at high LiDAR frequencies (>100 Hz).
- **Direct odometry:** Registers raw points directly to the map (no feature extraction) — more robust in low-feature environments.
- **Tightly-coupled fusion:** LiDAR + IMU via Iterative Extended Kalman Filter (IEKF).
- **Sensor support:** Rotating LiDARs (Hesai, Velodyne, Ouster) and solid-state (Livox).
- **Config for Hesai XT16:** Use `src/fast_lio_ros2/config/hesai_xt16.yaml` (created 2026-03-31). To adapt for other sensors: tune `extrinsic_T`, `extrinsic_R`, `scan_line`, `lidar_type`, and IMU noise params (`gyr_cov`, `acc_cov`, `b_gyr_cov`, `b_acc_cov`).


Wi-Fi via Alfa AWUS036ACM dongle on the Dock is the primary path. If the driver is incompatible
with the Orin kernel (ARM64), fallback options in priority order:
1. **Travel router** (GL.iNet) on the Go2 Ethernet port — transparent L2 bridging, no driver issues
2. **Tethered mode** — laptop connected via Ethernet to the Dock during demonstrations

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

## Known Issues & Troubleshooting

### Docker Container Startup
- **Issue:** The container exits immediately after `docker run` (Exit Code 0). ✅ **FIXED**
- **Cause:** `CMD ["/bin/bash"]` without an interactive TTY.
- **Fix applied:** `docker-compose.yml` now sets `stdin_open: true` and `tty: true`; use `docker compose up -d`.

### Build Failures on Robot (Orin)
- **`livox_ros_driver2` package.xml:** ✅ **FIXED** — `package.xml` (identical to `package_ROS2.xml`) is committed to the repo. No manual symlink needed.
- **Missing Dependencies (PCL):** ✅ **FIXED** — `Dockerfile` now includes `libpcl-dev` and `ros-humble-pcl-ros`.
- **CycloneDDS random interface:** ✅ **FIXED** — `docker/cyclonedds.xml` (with `eth0`) is mounted in the container via `CYCLONEDDS_URI`.
- **CycloneDDS socket receive buffer (Orin host):** `docker/cyclonedds.xml` requests a 4 MB buffer (`SocketReceiveBufferSize min="4194304"`). Requires `net.core.rmem_max ≥ 4194304` on the host. Persistent fix: `echo "net.core.rmem_max=4194304" | sudo tee /etc/sysctl.d/99-ros2-dds.conf && sudo sysctl --system`.
- **`rmw_cyclonedds_cpp` not installed:** ✅ **FIXED** — `ros-humble-rmw-cyclonedds-cpp` added to `Dockerfile`.

## TODO / Roadmap

### Next Session Tasks (Single-Command Startup — branch `feat/single-command-startup`)
Tasks 1–7 of the implementation plan are complete. Remaining:
- [ ] **[Validation]** End-to-end validation on robot: `./sync_to_dog.sh` → `docker compose up --build -d` → verify all nodes running (`ros2 node list`), TF chain, LiDAR topic Hz. Requires physical access to Orin + LiDAR powered on. See `docs/superpowers/plans/2026-04-09-single-command-startup.md` Task 8 for detailed steps.
- [ ] **[Tuning]** Nav2 `nav2_params.yaml` MPPI weights and time horizon are initial estimates — require field testing. Go2 footprint polygon (`robot_radius: 0.35`) is approximate — refine through iterative testing.
- [ ] Merge `feat/single-command-startup` → `main` after validation passes.

### General Roadmap
- [x] Integrate ROS 2 driver for Hesai XT16 LiDAR
- [x] Connect robot to laboratory Wi-Fi (ARSCONTROL)
- [x] Initialize and update all Git submodules (added `allan_variance_ros2` for IMU characterization)
- [x] Establish SSH access to Expansion Dock (`192.168.123.18`)
- [x] Diagnose MCU networking (DDS traffic not bridged to Wi-Fi)
- [ ] **[Hardware/Calibration]** Verify Hesai XT16 extrinsics physically (official values: T=[0.171,0,0.0908], R=I₃ — measure with caliper to confirm ±2 mm tolerance) and verify Wi-Fi dongle driver compatibility
- [ ] **[SLAM Tuning]** Characterize IMU noise (BMI088) via 6-minute static rosbag (recorded 2026-04-08) for initial IEKF white noise tuning; use datasheet fallbacks for bias random walk until a 3-hour capture is performed.
- [x] Create `cyclonedds.xml` with explicit `<NetworkInterface>` and mount it in Docker Compose via `CYCLONEDDS_URI`
- [x] Implement `go2_nav_bridge`: `cmd_vel` → `SportModeCmd` translation with watchdog, input sanitization, compiler hardening, and 21 GTest unit tests (all `ament_lint_auto` checks pass)
- [x] Verify `go2_nav_bridge` is `ament_cmake` (not `ament_python`) for correct `--symlink-install` behavior on YAML/launch files
- [x] Publish `base_link → hesai_lidar` static TF via `static_transform_publisher` in `bringup.launch.py` (extrinsics: T=[0.171, 0, 0.0908], R=I₃)
- [x] Configure FAST-LIO2 YAML for Hesai XT16 (`src/fast_lio_ros2/config/hesai_xt16.yaml` — official Unitree extrinsics; IMU noise from BMI088 datasheet, pending rosbag characterization)
- [x] Multi-stage Dockerfile: builder compiles workspace, runtime image launches `bringup.launch.py` via CMD
- [x] Docker Compose prod service (no `command: bash`) + dev profile with volume mount
- [x] Master launch file `bringup.launch.py` orchestrating full stack (Hesai → TF → FAST-LIO2 → Nav2 → bridge)
- [x] Configure Nav2 with `SmacPlannerHybrid` planner + MPPI controller (`nav2_params.yaml`)
- [x] Configure Nav2 lifecycle manager `node_names` — exclude `map_server` and `amcl` (FAST-LIO2 handles localization)
- [ ] **[SLAM Extension]** Integrate `octomap_server` or similar to provide 2D Occupancy Grid from FAST-LIO2 point cloud for Nav2
- [ ] Resolve wireless telemetry (Wi-Fi dongle on Dock or DDS Discovery Server)
- [ ] **[Baseline Validation]** Verify end-to-end navigation: LiDAR → FAST-LIO2 → Nav2 → bridge → physical motion
- [ ] **[Baseline Validation]** Conduct at least 3 trials on a measured path to establish quantitative RMSE/FLE baseline
- [ ] Build and deploy final ARM64 Docker image (`docker save/load`)

## graphify

This project has a graphify knowledge graph at graphify-out/.

Rules:
- Before answering architecture or codebase questions, read graphify-out/GRAPH_REPORT.md for god nodes and community structure
- If graphify-out/wiki/index.md exists, navigate it instead of reading raw files
- After modifying code files in this session, run `graphify update .` to keep the graph current (AST-only, no API cost)
