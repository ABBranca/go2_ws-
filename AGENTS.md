# AGENTS.md — Unitree Go2 Autonomous Navigation Stack

## Build Commands

```bash
# First-time setup
git submodule update --init --recursive

# Build entire workspace (inside Docker)
colcon build --symlink-install && source install/setup.bash

# Build single package
colcon build --symlink-install --packages-select go2_nav_bridge

# Clean rebuild
rm -rf build/ install/ log/ && colcon build --symlink-install
```

## Test & Lint Commands

```bash
# Run all package tests
colcon test --packages-select go2_nav_bridge && colcon test-result --verbose

# Run linters (ament_cppcheck, ament_cpplint, ament_uncrustify)
colcon test --packages-select go2_nav_bridge --ctest-args -R lint && colcon test-result --verbose

# Manual lint checks
ament_cppcheck src/go2_nav_bridge
ament_cpplint src/go2_nav_bridge
ament_uncrustify src/go2_nav_bridge
```

## Code Style (C++17)

### Formatting
- **Indentation**: 2 spaces, no tabs
- **Braces**: Allman style (opening brace on new line)
- **Line length**: 100 characters max
- **Trailing whitespace**: None
- **File encoding**: UTF-8

### Naming Conventions
- **Classes**: `PascalCase` (e.g., `Go2NavBridge`)
- **Functions/Methods**: `snake_case` (e.g., `cmd_vel_callback`)
- **Variables**: `snake_case` (e.g., `cmd_vel_sub_`)
- **Constants**: `kPascalCase` with `k` prefix (e.g., `kLinearMax`)
- **Private members**: `snake_case` with trailing underscore (e.g., `sdk_pub_`)
- **Files**: `snake_case.cpp` / `snake_case.hpp`

### Includes Order
1. Standard library (`<algorithm>`, `<cstdint>`)
2. ROS 2 headers (`<rclcpp/rclcpp.hpp>`)
3. Message headers (`<geometry_msgs/msg/twist.hpp>`)
4. Custom package headers (`<unitree_go/msg/sport_mode_cmd.hpp>`)

### Error Handling
- Use `RCLCPP_INFO/WARN/ERROR` macros for logging (not `printf`/`std::cout`)
- Use `RCLCPP_WARN_THROTTLE` for periodic warnings to avoid spam
- Validate parameters with `declare_parameter()` defaults
- Never crash on bad input — clamp or log and continue

### ROS 2 Patterns
- QoS: Match publisher/subscriber profiles (`RELIABLE`, depth 10 for cmd_vel)
- Use `std::bind` with `std::placeholders::_1` for callbacks
- Always call `rclcpp::init()` before node creation and `rclcpp::shutdown()` after
- Use `this->now()` for timestamps (not `std::chrono`)

## Python Style (for launch files, drivers)
- **PEP 8** compliant, 4-space indentation
- **Imports**: standard library → third-party → ROS 2 → local
- **Naming**: `snake_case` for functions/variables, `PascalCase` for classes

## Architecture

### ROS 2 Stack
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

### Network Topology
| Unit | IP | SSH | Role |
|---|---|---|---|
| MCU (Motion Control) | `192.168.123.161` | **blocked** | Motors, low-level SDK, Wi-Fi adapter |
| Expansion Dock (Orin) | `192.168.123.18` | enabled | Runs Docker + full ROS 2 stack |
| Developer Laptop | `192.168.123.10` | — | Code editing, Rviz2 visualization |
| Hesai XT16 LiDAR | `192.168.123.20` | — | UDP `2368` data, TCP `9347` PTC |

### Packages
| Package | Type | Role |
|---|---|---|
| `go2_nav_bridge` | local | Translates Nav2 `cmd_vel` → Unitree `SportModeCmd` |
| `unitree_ros2` | submodule | Unitree SDK + message types (`unitree_go`, `unitree_api`) |
| `fast_lio_ros2` | submodule (ROS2 branch) | Tightly-coupled LiDAR-IMU SLAM via IEKF + ikd-Tree |
| `hesai_ros_driver_2` | submodule | Official Hesai XT16 driver |
| `livox_ros_driver2` | submodule | Provides custom message types required by FAST-LIO2 |

### Control Layer Architecture (3T)
```
┌──────────────────────────────────────────────────────┐
│  DELIBERATIVE LAYER  (~0.1 Hz)                       │
│  LLM (external) → MCP Client → MCP Server (Orin)    │
│  Task planning, semantic grounding, goal generation  │
├──────────────────────────────────────────────────────┤
│  REACTIVE LAYER  (~20 Hz)                            │
│  Nav2 (costmap, planner, MPPI controller)           │
│  Obstacle avoidance, path following                   │
├──────────────────────────────────────────────────────┤
│  EXECUTIVE LAYER  (~50 Hz)                           │
│  go2_nav_bridge (cmd_vel → SportModeCmd)             │
│  Velocity clamping, watchdog, MCU interface         │
└──────────────────────────────────────────────────────┘
```

## Key Implementation Rules

- **Bridge node** (`src/go2_nav_bridge`): Use `SportModeCmd` (mode=2), NOT `LowCmd`
  - Mapping: `mode=2` (VelocityMove), `velocity[0]=linear.x`, `velocity[1]=linear.y`, `yaw_speed=angular.z`
  - Velocity limits: Clamp linear ±1.0 m/s, angular ±1.0 rad/s
- **TF tree**: `map → odom → base_link → hesai_lidar` (static: T=[0.171,0,0.0908], R=I₃)
- **CycloneDDS**: Must have explicit `<NetworkInterface name="eth0" />` config
- **Package type**: Use `ament_cmake` (not `ament_python`) for symlink support
- **Nav2 for quadruped**: Use `SmacPlannerHybrid` + MPPI controller (not NavFn + DWB)

## Launch Commands

```bash
# Hesai XT16 LiDAR driver
ros2 launch hesai_ros_driver_2 start.py

# FAST-LIO2 SLAM
ros2 launch fast_lio mapping.launch.py config_file:=hesai_xt16.yaml rviz:=true

# Visualize on laptop
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
```

## Deployment

### Sync to Robot
```bash
./sync_to_dog.sh   # rsync src/ → unitree@192.168.123.18
```

### Docker Deployment (Precompiled Image)
```bash
# Build locally (ARM64)
docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load .

# Transfer to robot
docker save go2_nav_stack:latest | ssh -C unitree@192.168.123.18 'docker load'

# Start on robot
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose up -d"
```

## TODO / Roadmap

### Build Fixes (Current Session)
- [ ] **[Fix]** Update `docker/Dockerfile` to include `libpcl-dev` and `ros-humble-pcl-ros` dependencies
- [ ] **[Fix]** Symlink `src/livox_ros_driver2/package_ROS2.xml` to `package.xml` for ROS 2 build compatibility
- [ ] **[Fix]** Rebuild Docker image locally: `docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load .`
- [ ] **[Fix]** Re-transfer the new Docker image to the robot and verify the container build on Orin

### General Roadmap
- [x] Integrate ROS 2 driver for Hesai XT16 LiDAR
- [x] Connect robot to laboratory Wi-Fi (ARSCONTROL)
- [x] Initialize and update all Git submodules
- [x] Establish SSH access to Expansion Dock (`192.168.123.18`)
- [x] Diagnose MCU networking (DDS traffic not bridged to Wi-Fi)
- [ ] **[Hardware]** Verify Hesai XT16 extrinsics physically (official values: T=[0.171,0,0.0908], R=I₃ — measure with caliper to confirm ±2 mm tolerance)
- [ ] **[Hardware]** Characterize IMU noise (BMI088) via 5-min static rosbag recording
- [ ] Create `cyclonedds.xml` with explicit `<NetworkInterface>` and mount in Docker Compose
- [ ] Implement `go2_nav_bridge`: `cmd_vel` → `SportModeCmd` translation
- [ ] Publish `base_link → hesai_lidar` static TF in the bridge launch file
- [x] Configure FAST-LIO2 YAML for Hesai XT16 (`src/fast_lio_ros2/config/hesai_xt16.yaml`)
- [ ] **[SLAM]** Integrate `octomap_server` for 2D Occupancy Grid from FAST-LIO2 point cloud
- [ ] Configure Nav2 with `SmacPlannerHybrid` planner + MPPI controller
- [ ] Configure Nav2 lifecycle manager `node_names` — exclude `map_server` and `amcl`
- [ ] Resolve wireless telemetry (Wi-Fi dongle on Dock or DDS Discovery Server)
- [ ] **[Baseline]** Verify end-to-end navigation: LiDAR → FAST-LIO2 → Nav2 → bridge → physical motion
- [ ] **[Baseline]** Conduct at least 3 trials on a measured path to establish quantitative RMSE/FLE baseline
- [ ] **[MCP]** Define spatial grounding map (`config/waypoints.yaml`) with named waypoints
- [ ] **[MCP]** Implement `mcp_watchdog` node: heartbeat monitor + Nav2 goal cancellation + safe-stop
- [ ] **[MCP]** Implement MCP Server on Expansion Dock with formal tool API
- [ ] **[MCP]** Profile Orin memory budget: FAST-LIO2 + Nav2 + VLM concurrent load (8 GB shared)
- [ ] Build and deploy final ARM64 Docker image

## Known Issues

### Docker Container Startup
- **Issue:** Container exits immediately after `docker run` (Exit Code 0).
- **Solution:** Always start with `-itd` flag: `docker run -itd --name go2_navigation ...`

### Build Failures on Robot (Orin)
- **`livox_ros_driver2` package.xml:** Expected `package.xml` but only `package_ROS1.xml` and `package_ROS2.xml` exist.
  - *Fix:* Symlink `src/livox_ros_driver2/package_ROS2.xml` to `package.xml`
- **Missing PCL:** `libpcl-dev` missing from image.
  - *Fix:* Update `docker/Dockerfile` to include `libpcl-dev` or `ros-humble-pcl-ros`

### CycloneDDS Network Interface
- **Issue:** CycloneDDS picks network interface randomly — causes intermittent node discovery failures.
- **Solution:** Always provide `cyclonedds.xml` with `<NetworkInterface name="eth0" />` and set `CYCLONEDDS_URI`.
