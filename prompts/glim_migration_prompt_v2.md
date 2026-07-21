# Role: Senior Robotics Engineer (Specialist ROS 2 and NVIDIA Jetson)
# Task: SLAM Migration from FAST-LIO2 to GLIM (GPU-accelerated) for Unitree Go2

## Context

Currently, the Unitree Go2 robot uses FAST-LIO2 for SLAM. FAST-LIO2 is a pure odometer — no loop closure, no dynamic `map→odom` publication. We migrate to **GLIM** (https://github.com/koide3/glim) to obtain:
- GPU-accelerated SLAM pipeline (CUDA on Jetson Orin)
- Loop closure with global consistency
- Full REP-105 compliance: `map→odom` published dynamically by the SLAM system

### Current Technical Data
- **LiDAR:** Hesai XT16 (16 channels, mechanical). Topic: `/lidar_points`.
- **IMU:** Integrated in Hesai driver. Expected topic: `/lidar_imu` (verified from rosbag; see §0 for live confirmation).
- **Extrinsics (base_link → hesai_lidar):** T=[0.171, 0, 0.0908], R=Identity (confirmed in `hesai_xt16.yaml` and physically).
- **Current TF Chain (FAST-LIO2 workaround):**
  ```
  map →[id]→ odom →[id]→ camera_init →[FAST-LIO2 dyn]→ body →[id]→ base_link →[static]→ hesai_lidar
  ```
- **Target TF Chain (GLIM):**
  ```
  map →[GLIM, on loop closure]→ odom →[GLIM, continuous]→ base_link →[static]→ hesai_lidar
  ```
- **Environment:** Ubuntu 22.04, ROS 2 Humble, JetPack 6.1 (CUDA 12.2+). Dockerfile is multi-stage (builder + runtime).
- **OctoMap:** Active in production stack. Consumes `/cloud_registered` (FAST-LIO2 output) to generate 2D occupancy grid for Nav2. Must remain functional after migration.
- **FAST-LIO2 status:** Already vendored as a plain directory (`src/fast_lio_ros2/`). Not a git submodule (converted in commit `f6255f8`). ROS2 package name: `fast_lio`.

## Objectives
Implement GLIM on a new branch, remove FAST-LIO2, and ensure Nav2 and OctoMap continue to function.

---

### §0. Pre-Migration Verification (do this before any changes)

```bash
# 0.1 Confirm live IMU topic and rate
ros2 topic list | grep -i imu
ros2 topic hz /lidar_imu        # expect ~200 Hz from Hesai integrated IMU
# If /lidar_imu is absent, check /utlidar/imu (Go2 MCU DDS source)

# 0.2 Confirm exact GLIM package names from PPA
curl -s https://koide3.github.io/ppa/setup_ppa.sh | sudo bash
apt-cache search glim | grep humble
apt-cache search gtsam-points
# Note the exact package names — use them throughout §2.1 and §3.1
```

---

### §1. Branch Management and Cleanup

```bash
# 1.1 Create branch
git checkout main && git checkout -b feature/glim-migration

# 1.2 Remove FAST-LIO2 vendored directory (NOT a submodule — no git submodule commands needed)
rm -rf src/fast_lio_ros2/
git rm -r src/fast_lio_ros2/

# 1.3 Remove build artifacts
rm -rf build/fast_lio install/fast_lio log/

# 1.4 Remove any find_package(fast_lio) references in CMakeLists.txt files
grep -rn "fast_lio" src/ --include="CMakeLists.txt" --include="package.xml"
# Edit found files to remove FAST-LIO2 dependencies
```

---

### §2. GLIM Installation and Configuration

#### §2.1 Installation

Use the exact package names identified in §0.2:

```bash
# Install GLIM ROS2 package (CUDA variant for JetPack 6.1 / CUDA 12.2)
sudo apt install ros-humble-glim-ros-<cuda_variant>

# Install GTSAM-Points (required by GLIM global mapping)
sudo apt install libgtsam-points-<cuda_variant>-dev
```

#### §2.2 JSON Configuration

Create directory: `src/go2_nav_bridge/config/glim/`

**`config_sensors.json`** — Hesai XT16 sensor:

```json
{
  "sensor_type": "velodyne",
  "velodyne_model": "VLP-16",
  "lidar_topic": "/lidar_points",
  "imu_topic": "/lidar_imu",
  "T_lidar_imu": [0.171, 0.0, 0.0908, 1.0, 0.0, 0.0, 0.0]
}
```

Notes:
- `sensor_type: "velodyne"` — Hesai XT16 uses Velodyne-compatible packet format (same as `lidar_type: 2` in FAST-LIO2 config).
- `T_lidar_imu` format shown: [tx, ty, tz, qw, qx, qy, qz] — identity rotation (R=I₃). ⚠️ GLIM may expect a 4×4 matrix or [x, y, z, roll, pitch, yaw] depending on version. Verify from GLIM example configs before deploying.
- ⚠️ Verify exact key names and extrinsic format against installed version: check `~/.ros/glim/` default configs or GLIM documentation at https://github.com/koide3/glim/wiki/Configuration

**`config.json`** — GPU pipeline with headless mode:

```json
{
  "enable_viewer": false,
  "odometry_type": "gpu",
  "sub_mapping_type": "gpu",
  "global_mapping_type": "gpu",
  "base_frame": "base_link",
  "odom_frame": "odom",
  "map_frame": "map",
  "save_on_exit": true,
  "map_save_path": "/ros2_ws/glim_map"
}
```

`enable_viewer: false` is **mandatory** — Docker containers have no display. GLIM uses GLFW for its viewer; omitting this flag causes a crash at startup (see koide3/glim #133, #183).

#### §2.3 TF Chain After Migration

GLIM publishes (with config above):
- `odom → base_link` (continuous, every odometry update)
- `map → odom` (event-driven, only when loop closure fires)

**Remove** from `bringup.launch.py`:
- `tf_map_odom` (`map → odom` static identity) — GLIM publishes this dynamically
- `tf_odom_camera_init` (`odom → camera_init`) — frame no longer exists
- `tf_body_base_link` (`body → base_link`) — frame no longer exists

**Keep** in `bringup.launch.py`:
- `tf_base_link_hesai` (`base_link → hesai_lidar`, T=[0.171, 0, 0.0908]) — still required; GLIM does not publish sensor-to-base transforms

#### §2.4 OctoMap Topic Remapping (CRITICAL)

FAST-LIO2 publishes registered points on `/cloud_registered`. GLIM publishes on a different topic (verify with `ros2 topic list` after GLIM starts — likely `/glim_ros/cloud` or `/accum_points`).

Update `src/go2_nav_bridge/launch/octomap.launch.py`:
```python
# Replace the cloud_registered remapping:
remappings=[('/cloud_in', '/glim_ros/cloud')]   # use verified topic name
```

Without this fix: OctoMap receives zero points → occupancy grid empty → Nav2 global planner fails.

---

### §3. Docker Update

#### §3.1 Dockerfile — Both Stages

The Dockerfile has two stages (`builder` and `runtime`). Add PPA + GLIM packages to **both**:
- **builder stage**: needs GLIM headers for `colcon build` of any package that links against GLIM
- **runtime stage**: needs GLIM shared libraries for execution

```dockerfile
# Add to BOTH stages, after the FROM line:
RUN curl -s https://koide3.github.io/ppa/setup_ppa.sh | bash && \
    apt-get update && apt-get install -y \
      ros-humble-glim-ros-<cuda_variant> \
      libgtsam-points-<cuda_variant>-dev
```

#### §3.2 docker-compose.yml — NVIDIA Runtime

Add GPU access to **both** `go2_navigation` and `go2_navigation_dev` services:

```yaml
deploy:
  resources:
    reservations:
      devices:
        - driver: nvidia
          count: 1
          capabilities: [gpu]
```

Verify GPU access inside container: `docker exec go2_navigation nvidia-smi`

---

### §4. Launch File Updates

#### `src/go2_nav_bridge/launch/bringup.launch.py`

```python
# Remove:
fast_lio_dir = get_package_share_directory('fast_lio')   # package gone
# Remove: tf_map_odom, tf_odom_camera_init, tf_body_base_link

# Add:
glim_node = Node(
    package='glim_ros',
    executable='glim_rosnode',
    name='glim_ros',
    parameters=[{
        'config_path': os.path.join(go2_nav_bridge_dir, 'config', 'glim'),
    }],
    output='screen',
)

# Keep: tf_base_link_hesai, hesai_launch, octomap_launch, nav2_launch
```

#### `src/go2_nav_bridge/launch/mapping_only.launch.py`

- Same FAST-LIO2 → GLIM node substitution.
- Update docstring diagram to reflect new topic graph:
  ```
  [Hesai driver] ──/lidar_points──┐
                                   ├──> [GLIM] ──/glim_ros/cloud──> [octomap]
  [Hesai IMU] ──/lidar_imu────────┘         └──/tf (map→odom, odom→base_link)
  ```

---

### §5. Rollback Plan

If GLIM fails (CUDA incompatibility, unresolved GLFW issue, insufficient performance):

```bash
# Option A: return to FAST-LIO2
git checkout main

# Option B: hybrid fallback (FAST-LIO2 frontend + PGO loop closure, no mandatory CUDA)
# https://github.com/liangheming/fastlio2_ros2
# Add as vendored directory in src/; requires minimal TF chain changes
```

---

### §6. Verification Sequence

Run after full stack launch:

```bash
# 1. TF tree integrity
ros2 run tf2_tools view_frames
# Expect: map → odom → base_link → hesai_lidar
# odom→base_link must be dynamic (GLIM), base_link→hesai_lidar static

# 2. GLIM receiving sensor data
ros2 topic hz /lidar_points    # expect ~10 Hz
ros2 topic hz /lidar_imu       # expect ~200 Hz

# 3. Odometry publishing
ros2 topic echo /odom --once

# 4. map→odom is dynamic (loop closure active)
ros2 topic echo /tf | grep -A5 '"map"'
# Timestamps must update continuously; a frozen timestamp = static bridge still present

# 5. GPU utilization (GLIM using CUDA)
tegrastats | grep GR3D         # expect > 0% during active SLAM

# 6. OctoMap still generating occupancy grid
ros2 topic hz /projected_map   # must not be 0 Hz or absent

# 7. Nav2 receiving valid costmap
ros2 topic echo /global_costmap/costmap_updates --once
```

---

## Constraints
- Do not modify `go2_nav_bridge` control logic (Watchdog, `cmd_vel` sanitization, bridge node).
- Maintain `cyclonedds` as RMW middleware (`RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` in compose env).
- `enable_viewer: false` in `config.json` is non-negotiable for Docker deployment.
- Document every file created or modified in `SESSION_DIARY.md` (workspace root), format:
  ```
  | date       | file                          | action  | reason                        |
  |------------|-------------------------------|---------|-------------------------------|
  | 2026-05-19 | src/go2_nav_bridge/config/... | created | GLIM sensor configuration     |
  ```
