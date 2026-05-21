# lidarslam_ros2 Integration — Design Spec

**Date:** 2026-05-21
**Branch:** `feature/lidarslam-ros2-experiment` (from `feature/fast-lio2-restore-rep105`)
**Goal:** Parallel experiment: integrate `rsasaki0109/lidarslam_ros2` as alternative SLAM on Go2 + Hesai XT16. Compare odometry quality against FAST-LIO2.

---

## 1. Context

`lidarslam_ros2` was previously tested on Go2 and produced a valid map, but with slightly off odometry. Root cause unknown — this branch investigates with proper IMU integration and tuned parameters.

**Stack:** ROS 2 Humble, Docker ARM64 (Orin NX), Hesai XT16 LiDAR, Go2 built-in IMU (`/utlidar/imu`).

FAST-LIO2 source (`src/fast_lio_ros2/`) is left untouched — this is a swap at the launch level only.

---

## 2. Architecture

### TF Chain (REP-105 compliant)

```
map ─[map_odom_broadcaster, 50 Hz, identity]──> odom
    ─[pose_relay, ~10 Hz, from /current_pose]──> base_link
    ─[static extrinsic, T=(0.171, 0, 0.0908)]──> hesai_lidar
```

### Data Flow

```
Hesai /lidar_points ──remap──> /input_cloud ──> lidarslam_ros2 (frontend)
/utlidar/imu        ──remap──> /imu         ──> lidarslam_ros2 (distortion correction)

lidarslam_ros2 /current_pose (PoseStamped, frame: map)
    └──> pose_relay node ──> TF odom→base_link

/lidar_points + TF odom→base_link + TF base_link→hesai_lidar
    └──> octomap_server (raw scan + TF lookup → occupancy grid for Nav2)
```

### Key Design Decision: `publish_tf: false`

lidarslam publishes `map→base_link` by default. This conflicts with our two-node chain (`map_odom_broadcaster` + `pose_relay`). Setting `publish_tf: false` and delegating to `pose_relay` avoids the TF conflict and preserves the REP-105 chain.

### Octomap Input

FAST-LIO2 published pre-registered `/cloud_registered` (already in map frame). lidarslam has no equivalent per-scan output — only an accumulated `/map` at low frequency. Octomap is fed `/lidar_points` (raw sensor data) instead; it uses TF internally to project scans into the map frame. Update rate is ~10 Hz (LiDAR scan rate), acceptable for navigation.

---

## 3. New Files

### `src/go2_nav_bridge/src/pose_relay.cpp`

Minimal C++ node:
- **Subscribes:** `/current_pose` (`geometry_msgs/PoseStamped`, published by lidarslam frontend, frame_id: `map`)
- **Publishes:** TF `odom → base_link` (TransformStamped, timestamp from incoming message)
- **Parameters:** `odom_frame` (default: `"odom"`), `base_frame` (default: `"base_link"`)
- **No interpolation, no buffering** — each pose message maps 1:1 to a TF broadcast

Why `odom→base_link` and not `map→base_link`: avoids conflict with `map_odom_broadcaster` which already owns the `map→odom` edge. Nav2 resolves the full chain correctly.

### `src/go2_nav_bridge/config/lidarslam/params.yaml`

```yaml
/**:
  ros__parameters:
    registration_method: "NDT"
    ndt_resolution: 2.0          # smaller than default (5.0) for XT16 sparse cloud
    ndt_num_threads: 0           # use all available cores
    scan_period: 0.1             # 10 Hz — required when use_imu: true
    use_imu: true
    vg_size_for_input: 0.2
    vg_size_for_map: 0.05
    scan_max_range: 20.0         # XT16 practical max
    scan_min_range: 0.5
    trans_for_mapupdate: 1.0
    num_targeted_cloud: 10
    publish_tf: false            # pose_relay handles TF
    map_publish_period: 15.0
    debug_flag: false
```

**Tuning notes:**
- If NDT diverges: increase `ndt_resolution` to 3.0–5.0
- If map update is too slow: reduce `trans_for_mapupdate` to 0.5
- `use_imu: true` requires `/imu` with angular_velocity + linear_acceleration + orientation (9-axis). Go2 `/utlidar/imu` provides all three.

---

## 4. Modified Files

### `src/go2_nav_bridge/CMakeLists.txt`

Add `pose_relay` executable (after `map_odom_broadcaster` block):

```cmake
add_executable(pose_relay src/pose_relay.cpp)
ament_target_dependencies(pose_relay rclcpp geometry_msgs tf2_ros tf2_geometry_msgs)
```

Add to install(TARGETS):
```cmake
install(TARGETS bridge_node map_odom_broadcaster pose_relay ...)
```

### `src/go2_nav_bridge/launch/bringup.launch.py`

Replace FAST-LIO2 launch block with lidarslam. Key changes:
- Remove `fast_lio_launch`, `tf_odom_camera_init`, `tf_body_base_link`
- Add `lidarslam_launch` (with topic remappings: `/lidar_points`→`/input_cloud`, `/utlidar/imu`→`/imu`)
- Add `pose_relay` node
- Keep `map_odom_broadcaster`, `tf_base_link_hesai`, `hesai_launch`, `nav2_launch`, `octomap_launch`, `bridge_node`
- Remap octomap `cloud_in` from `/cloud_registered` to `/lidar_points`

### `docker/Dockerfile`

Builder stage: minimal changes — most deps overlap with FAST-LIO2:
- **Keep:** `libomp-dev`, `libpcl-dev`, `ros-humble-pcl-ros` (used by ndt_omp_ros2 and lidarslam)
- **Remove:** `ros-humble-eigen-stl-containers`, `ros-humble-pcl-conversions` (FAST-LIO2 specific, not needed by lidarslam)
- `lidarslam_ros2` vendored with `ndt_omp_ros2` submodule — no additional apt deps beyond existing PCL + OpenMP

Runtime stage: no changes expected (PCL + OpenMP runtime libs already present from FAST-LIO2 setup).

---

## 5. Vendoring lidarslam_ros2

```bash
cd src/
git clone --recursive https://github.com/rsasaki0109/lidarslam_ros2.git
cd lidarslam_ros2
git rev-parse HEAD > .pinned_commit
rm -rf .git
# ndt_omp_ros2 submodule also loses .git — vendored as flat directory
```

Package name in ROS 2: `lidarslam` (not `lidarslam_ros2`). Launch file: `lidarslam.launch.py`.

---

## 6. Out of Scope

- Loop closure tuning (backend graph SLAM params)
- GICP vs NDT A/B comparison (start with NDT)
- `robot_localization` EKF integration
- Map saving / persistence (`/map_save` service)
- Merging back to `feature/fast-lio2-restore-rep105`

---

## 7. Success Criteria

1. Docker ARM64 build succeeds with lidarslam_ros2 compiled
2. TF chain `map→odom→base_link→hesai_lidar` visible in `ros2 run tf2_tools view_frames`
3. `/current_pose` published at ~10 Hz during robot motion
4. Octomap populates with obstacles visible in RViz
5. Nav2 accepts a 2D goal and plans a path (MPPI controller active)
6. Subjective odometry quality comparison vs FAST-LIO2 branch
