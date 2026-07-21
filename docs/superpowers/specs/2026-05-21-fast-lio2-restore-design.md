# FAST-LIO2 Restore with Dynamic map→odom Broadcaster

**Date:** 2026-05-21
**Branch:** `feature/glim-migration` (will be renamed / superseded)
**Status:** Approved — proceed to implementation plan

## Context

Migration to GLIM (commits `8e397b4`, `4a959d6`) introduced unrecoverable SLAM
divergence on the Go2 / Hesai XT16 platform. Root causes diagnosed in session:

- VGICP front-end degenerate on sparse 16-beam LiDAR with default config
  (`distance_near_thresh=1.0`, `ivox_resolution=1.0`, `target_downsampling_rate=0.1`)
- IMU initialization in `LOOSE` mode with `fix_imu_bias: false` → bias divergence
- Global mapping creating spurious loop closures (`max_implicit_loop_distance=100.0`)
  with `min_implicit_loop_overlap=0.2` → `map→odom` jumps in RViz

Symptoms observed (RViz screenshots, session date 2026-05-21):
- Image 1 (global mapping ON): rotational fan of odometry arrows at fixed
  location → spurious loop closures jerking `map→odom`
- Image 2 (global mapping OFF): trajectory drift across large area → front-end
  cannot constrain pose, IMU integration dominates

Tuning attempts (`distance_near_thresh=0.3`, `ivox_resolution=0.5`,
`initialization_mode=STATIC`) did not restore stable operation.

**Decision:** revert to FAST-LIO2, retain REP-105 compliance with a dynamic
`map→odom` broadcaster node (upgradeable to loop closure later without
launch-file changes).

## Goals

1. Restore navigation pipeline to end-to-end working state on Go2 / Hesai XT16.
2. Maintain REP-105 frame chain `map → odom → base_link → hesai_lidar`.
3. Make `map→odom` publishing path upgradeable to loop closure / relocalization
   without invasive refactor.

## Non-Goals

- Loop closure or relocalization implementation (deferred — node is a stub
  ready for it).
- Modification of FAST-LIO2 C++ source (use frame bridges to avoid patching).
- Performance tuning of FAST-LIO2 IMU/LiDAR covariances (keep last known
  working values from commit `df5776e`).

## Architecture

### TF Chain

```
map →[map_odom_broadcaster, identity, 50 Hz]→ odom
    →[static identity]→ camera_init
    →[FAST-LIO2 laserMapping, ~100 Hz IMU-rate]→ body
    →[static identity]→ base_link
    →[static extrinsic T=(0.171, 0, 0.0908), R=I]→ hesai_lidar
```

Rationale for keeping `odom → camera_init` and `body → base_link` static
identities: FAST-LIO2 hardcodes `camera_init` and `body` frame names in
`laserMapping.cpp`. Patching source is out of scope (non-goal). Static identity
bridges reconcile FAST-LIO2 internal frames with REP-105 standard names with
zero overhead.

### Components

#### 1. `src/fast_lio_ros2/` (restored package)

Clone HKU-Mars FAST_LIO ros2 branch into `src/fast_lio_ros2/`. Restore
`config/hesai_xt16.yaml` byte-identical to commit `df5776e` (known-working
IMU noise covariances, extrinsics `T=[0.171, 0, 0.0908]`, `R=I₃`,
`lidar_type=2`, `N_SCANS=16`).

Topic interface (unchanged from previous setup):
- Subscribes: `/lidar_points` (sensor_msgs/PointCloud2), `/utlidar/imu`
  (sensor_msgs/Imu)
- Publishes: `/Odometry` (nav_msgs/Odometry, frame `camera_init`),
  `/cloud_registered` (sensor_msgs/PointCloud2, frame `camera_init`),
  TF `camera_init → body`

#### 2. `go2_nav_bridge/go2_nav_bridge/map_odom_broadcaster.py` (new)

ROS 2 Python node. Single responsibility: publish `TransformStamped`
`map → odom` at 50 Hz.

Initial behavior: identity transform (translation zero, quaternion identity).

Forward-compatibility hook: subscribe to `/initialpose`
(geometry_msgs/PoseWithCovarianceStamped). On message receipt, update internal
state used as the published transform. **No** correction logic implemented now
— subscriber is a placeholder so the public interface is stable when loop
closure is added.

Parameters:
- `publish_rate_hz` (default 50.0)
- `map_frame` (default `map`)
- `odom_frame` (default `odom`)

#### 3. `src/go2_nav_bridge/launch/bringup.launch.py`

Restore previous structure with one change: the `_static_tf('map', 'odom')`
ExecuteProcess is replaced by a `Node(package='go2_nav_bridge',
executable='map_odom_broadcaster', ...)`.

GLIM node removed. Static bridges restored:
- `tf_odom_camera_init = _static_tf('odom', 'camera_init')`
- `tf_body_base_link = _static_tf('body', 'base_link')`
- `tf_base_link_hesai = _static_tf('base_link', 'hesai_lidar', x='0.171', z='0.0908')`

`base_link → utlidar_imu` static (from current GLIM launch) is removed —
FAST-LIO2 uses LiDAR-frame IMU extrinsics via `extrinsic_T`/`extrinsic_R` in
its YAML; no TF dependency on `utlidar_imu`.

`fast_lio_launch` IncludeLaunchDescription restored with
`config_file='hesai_xt16.yaml'`, `rviz='false'`.

#### 4. `src/go2_nav_bridge/setup.py`

Add console-script entry point:
```
'map_odom_broadcaster = go2_nav_bridge.map_odom_broadcaster:main',
```

#### 5. `docker/Dockerfile`

Remove blocks at lines 35-39 (builder stage GLIM PPA) and 95-99 (runtime stage
GLIM PPA). Add FAST-LIO2 build dependencies (apt packages):
- `ros-humble-pcl-ros`
- `ros-humble-eigen-stl-containers`
- `libpcl-dev`
- `libomp-dev`

Verify against `src/fast_lio_ros2/package.xml` exec_depend list after clone.

#### 6. `docker/docker-compose.yml`

Remove from both `go2_navigation` and `dev` services:
- `runtime: nvidia`
- `NVIDIA_VISIBLE_DEVICES=all` env var
- `NVIDIA_DRIVER_CAPABILITIES=compute,utility` env var

FAST-LIO2 is CPU-only — no GPU runtime requirement.

## Data Flow

```
/lidar_points (10 Hz)  ┐
                       ├──→ FAST-LIO2 ──→ TF camera_init→body (~100 Hz)
/utlidar/imu  (252 Hz) ┘            └──→ /Odometry, /cloud_registered

map_odom_broadcaster ──→ TF map→odom (50 Hz, identity)

/cloud_registered ──→ octomap_server ──→ /projected_map

Nav2 stack uses: TF map→base_link (via chain), /projected_map (costmap source)

cmd_vel (Nav2) ──→ go2_nav_bridge ──→ SportModeCmd (MCU)
```

## Error Handling

`map_odom_broadcaster`:
- No external state dependencies; cannot fail at startup beyond rclpy init.
- Publishing failure (TF broadcaster) is logged but node continues — ROS 2 TF
  broadcaster does not raise on transient failures.

FAST-LIO2 failures (insufficient IMU init, LiDAR topic missing) are surfaced
via stderr and are diagnosed externally — out of scope for this spec.

## Testing

Manual end-to-end verification (no automated test suite for ROS 2 SLAM
pipelines in this project):

1. **Build:** `colcon build --packages-select fast_lio go2_nav_bridge` exits 0.
2. **TF chain:** `ros2 run tf2_tools view_frames` shows `map → odom →
   camera_init → body → base_link → hesai_lidar` connected.
3. **Rates:** `ros2 topic hz /tf` shows ≥ 50 Hz for `map→odom`, ≥ 50 Hz for
   `camera_init→body`.
4. **RViz:** point cloud accumulated in `map` frame is geometrically coherent
   (no scatter, walls visible). Odometry arrows form a contiguous trajectory.
5. **Nav2:** `2D Goal Pose` triggers planning; robot executes path.

## Open Risks

- FAST-LIO2 upstream ROS2 branch may have diverged since commit `df5776e`.
  Mitigation: pin to a specific commit hash determined during implementation.
- `ros-humble-pcl-ros` / `libomp-dev` may have ABI conflicts with ARM64 cross
  build. Mitigation: incremental Docker build with `--target builder` first.

## Out of Scope

- AMCL or laser-based 2D relocalization.
- Multi-session map persistence.
- Loop closure pose-graph backend (deferred; `map_odom_broadcaster` ready for it).
