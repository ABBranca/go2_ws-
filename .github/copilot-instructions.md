# Copilot Instructions for go2_ws

This is the **Unitree Go2 Autonomous Navigation Stack** — a ROS 2 workspace integrating LiDAR-based SLAM, navigation planning, and custom Go2 control bridging.

## Build Commands

**Initialize workspace (first time only):**
```bash
git submodule update --init --recursive
```

**Build entire workspace** (inside Docker container on robot):
```bash
docker exec -it go2_navigation bash
colcon build --symlink-install
source install/setup.bash
```

**Build a single package** (faster for local iteration):
```bash
colcon build --symlink-install --packages-select go2_nav_bridge
```

**Build for final deployment** (ARM64 Docker image):
```bash
cd docker && docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load .
docker save go2_nav_stack:latest | ssh -C unitree@192.168.123.18 'docker load'
```

## Testing & Linting

**Lint a package** (ROS 2 conventions):
```bash
ament_lint_auto [package_name]
```

The project uses standard ament linters (cppcheck, flake8, pep257) enabled via `ament_lint_common` in `package.xml`.

## Key Architecture

### High-Level Data Flow
```
Hesai XT16 LiDAR (UDP 2368)
    ↓
hesai_ros_driver_2 → /lidar_points (PointCloud2)
    ↓
FAST-LIO2 (LiDAR-IMU SLAM)
    ↓
TF tree: map → odom → base_link → lidar_link
    ↓
Nav2 Stack (navigation planning)
    ↓
go2_nav_bridge (cmd_vel → SportModeCmd)
    ↓
Unitree Go2 MCU (motion control)
```

### Core Packages

| Package | Type | Role |
|---------|------|------|
| `go2_nav_bridge` | Local | Translates Nav2 `cmd_vel` to Unitree `SportModeCmd`. Currently a stub — bridge logic not yet implemented. |
| `unitree_ros2` | Submodule | Official Unitree ROS 2 SDK; provides message types (`unitree_go`, `unitree_api`). |
| `fast_lio_ros2` | Submodule (ROS2 branch) | LiDAR-IMU SLAM using direct point registration, ikd-tree, and IEKF fusion. |
| `hesai_ros_driver_2` | Submodule | Official Hesai XT16 driver; publishes PointCloud2 to `/lidar_points`. |
| `livox_ros_driver2` | Submodule | Custom message types required by FAST-LIO2; includes manually-added `package.xml` for ament compatibility. |

### Execution Environment

- **ROS 2 Distribution**: Humble
- **Middleware**: CycloneDDS
- **ROS_DOMAIN_ID**: 1
- **Container**: `go2_navigation` (runs on Expansion Dock at 192.168.123.18)
- **Hostname resolution**: Network mode is `host` (docker-compose.yml)

## Development Workflow

### Local Development Cycle

1. **Edit locally** on your laptop (modify files in `src/`)

2. **Sync to robot** via Ethernet (rsync via provided script):
   ```bash
   chmod +x sync_to_dog.sh
   ./sync_to_dog.sh
   ```
   This sends only changed files to `/home/unitree/go2_ws` on the Expansion Dock.

3. **Build on robot** (inside Docker):
   ```bash
   docker exec -it go2_navigation bash
   colcon build --symlink-install --packages-select [package_name]
   source install/setup.bash
   ```
   Python and YAML changes are instant thanks to `--symlink-install`.

4. **Visualize remotely** on your laptop:
   ```bash
   source /opt/ros/humble/setup.bash
   export ROS_DOMAIN_ID=1
   rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
   ```

### Docker Operations

**Start/restart the container** (first time):
```bash
cd docker && docker compose up --build -d
```

**Access the running container**:
```bash
docker exec -it go2_navigation bash
```

## Key Conventions & Patterns

### go2_nav_bridge Node

- **Entry point**: `src/go2_nav_bridge/src/bridge_node.cpp`
- **Subscription**: `cmd_vel` (geometry_msgs/Twist) from Nav2
- **Publication**: Unitree control messages (currently stubbed with `unitree_go::msg::LowCmd`)
- **Status**: Bridge logic (`Twist → SportModeCmd` mapping) is not yet implemented; see TODOs in bridge_node.cpp.
- **Preferred message type**: Use `SportModeCmd` (high-level) instead of `LowCmd` (joint-level) — leverages onboard stability controllers.

### FAST-LIO2 Configuration

FAST-LIO2 requires extrinsic calibration and IMU noise tuning for each LiDAR model:

- **Config location**: `src/fast_lio_ros2/config/` (YAML files, e.g., `mid360.yaml`)
- **Key parameters to tune**:
  - `extrinsic_T`: Translation between IMU and LiDAR frames
  - `extrinsic_R`: Rotation matrix for the same transform
  - `gyr_cov`, `acc_cov`: Gyroscope and accelerometer noise covariances
  - `b_gyr_cov`, `b_acc_cov`: Bias covariances
- **Status**: No native Hesai XT16 config exists; must adapt from an existing template.

### Transform Tree

Maintained across `FAST-LIO2` and custom static transforms:

- `map` → `odom`: Provided by FAST-LIO2 odometry node
- `odom` → `base_link`: Provided by FAST-LIO2 odometry node
- `base_link` → `lidar_link`: Static transform (needs manual calibration)

### Network & IP Configuration

| Unit | IP | SSH | Role |
|------|----|----|------|
| Motion Control Unit (MCU) | `192.168.123.161` | Blocked | Motors, low-level SDK, Wi-Fi adapter |
| Expansion Dock (Orin) | `192.168.123.18` | Enabled | Runs Docker, ROS 2 stack |
| Developer Laptop | `192.168.123.10` | — | Code editing, Rviz2 visualization |
| Hesai XT16 LiDAR | `192.168.1.201` | — | UDP 2368 data, TCP 9347 PTC |

**Networking limitation**: The MCU does not bridge DDS traffic from Ethernet (192.168.123.x) to Wi-Fi. SSH on MCU is blocked, so NAT cannot be configured there. Proposed solutions:
- USB Wi-Fi dongle on Dock (Alfa AWUS036ACM recommended)
- DDS Discovery Server
- WebRTC/DDS hybrid (see `abizovnuralem/go2_ros2_sdk` on GitHub)

## Common Tasks

### Run FAST-LIO2 SLAM
```bash
ros2 launch fast_lio mapping.launch.py config_file:=mid360.yaml rviz:=true
```

### Run Hesai LiDAR Driver
```bash
# Note: start.launch is ROS 1 XML; ROS 2 Python conversion is needed
ros2 launch hesai_ros_driver_2 start.py
```

### Check/Debug Message Types
The project uses Unitree's custom message types. Inspect available messages:
```bash
ros2 msg list | grep unitree
```

### Update Submodules
If submodules are out of sync:
```bash
git submodule update --recursive --remote
```

## Files You're Most Likely to Edit

- **`src/go2_nav_bridge/src/bridge_node.cpp`**: Main bridge logic (Twist translation)
- **`src/fast_lio_ros2/config/*.yaml`**: FAST-LIO2 extrinsic and IMU calibration
- **`docker/docker-compose.yml`**: ROS environment variables and container setup
- **`docker/Dockerfile`**: Dependencies and ROS 2 base image
- **`sync_to_dog.sh`**: Robot IP and sync paths (if network topology changes)

## Troubleshooting Notes

- **Build failures in submodules**: Ensure `git submodule update --init --recursive` was run after cloning.
- **livox_ros_driver2 compilation**: This submodule required a manual `package.xml` addition; it's already in the repo.
- **CMake errors on old systems**: Dockerfile targets ROS Humble on Ubuntu 22.04; verify your system matches.
- **DDS discovery not reaching MCU**: The MCU's Wi-Fi adapter doesn't bridge DDS. Use a USB Wi-Fi dongle on the Dock or reconfigure networking via travel router.

## Related Documentation

- **Local development guidance**: See CLAUDE.md (Claude-specific, but workflows apply to all editors)
- **Full setup and deployment**: See README.md for network topology, hardware requirements, and step-by-step instructions
- **External resources**:
  - [Unitree Go2 Community Nav Example](https://github.com/Sayantani-Bhattacharya/unitree_go2_nav)
  - [WebRTC Go2 SDK](https://github.com/abizovnuralem/go2_ros2_sdk)
  - [FAST-LIO2 GitHub](https://github.com/hku-mars/FAST_LIO)
