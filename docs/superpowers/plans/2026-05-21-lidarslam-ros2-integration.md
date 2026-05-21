# lidarslam_ros2 Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate `rsasaki0109/lidarslam_ros2` as alternative SLAM on the Go2 + Hesai XT16 stack; bridge to REP-105 via a `pose_relay` node so Nav2 can consume the resulting TF chain unchanged.

**Architecture:** New branch from `feature/fast-lio2-restore-rep105`. Vendor `lidarslam_ros2` (with `ndt_omp_ros2` submodule). Add C++ `pose_relay` node that converts lidarslam's `/current_pose` to a TF `odom → base_link`, while the existing `map_odom_broadcaster` continues to publish `map → odom` identity. Swap launch graph; minimal Docker changes (PCL + OpenMP already present from FAST-LIO2 setup).

**Tech Stack:** ROS 2 Humble, lidarslam_ros2 (NDT/GICP + graph SLAM), `ndt_omp_ros2`, C++17, ament_cmake, tf2_ros, Docker (ARM64 cross), Hesai XT16, Go2 `/utlidar/imu`.

**Spec:** `docs/superpowers/specs/2026-05-21-lidarslam-ros2-integration-design.md`

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `src/lidarslam_ros2/` | create (clone+vendor) | SLAM front-end (NDT/GICP) + backend (graph SLAM) |
| `src/lidarslam_ros2/ndt_omp_ros2/` | create (submodule, vendored) | OpenMP-accelerated NDT registration |
| `src/go2_nav_bridge/src/pose_relay.cpp` | create | `/current_pose` → TF `odom→base_link` |
| `src/go2_nav_bridge/CMakeLists.txt` | modify | Add `pose_relay` executable |
| `src/go2_nav_bridge/config/lidarslam/params.yaml` | create | NDT + IMU config, `publish_tf: false` |
| `src/go2_nav_bridge/launch/bringup.launch.py` | rewrite | Replace FAST-LIO2 → lidarslam + pose_relay |
| `docker/Dockerfile` | modify | Remove `ros-humble-eigen-stl-containers`, `ros-humble-pcl-conversions` (FAST-LIO2-only) |

---

## Task 1: Create Experiment Branch

**Files:** (none — git operation only)

- [ ] **Step 1: Verify current branch and clean working tree**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git status
git branch --show-current
```

Expected: branch is `feature/fast-lio2-restore-rep105`, working tree clean.

If working tree dirty, stop and ask the user how to proceed.

- [ ] **Step 2: Create and switch to new branch**

```bash
git checkout -b feature/lidarslam-ros2-experiment
git branch --show-current
```

Expected: output `feature/lidarslam-ros2-experiment`.

---

## Task 2: Vendor lidarslam_ros2

**Files:**
- Create: `src/lidarslam_ros2/` (cloned)
- Create: `src/lidarslam_ros2/ndt_omp_ros2/` (submodule, vendored)

- [ ] **Step 1: Clone repository with submodules**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws/src
git clone --recursive https://github.com/rsasaki0109/lidarslam_ros2.git
```

Expected: directory `src/lidarslam_ros2/` exists with subdirectories `lidarslam`, `lidarslam_components`, `lidarslam_msgs`, and `ndt_omp_ros2`.

- [ ] **Step 2: Pin upstream commit hashes for reproducibility**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws/src/lidarslam_ros2
LIDARSLAM_SHA=$(git rev-parse HEAD)
NDT_OMP_SHA=$(cd ndt_omp_ros2 && git rev-parse HEAD)
cat > .pinned_commit <<EOF
lidarslam_ros2: ${LIDARSLAM_SHA}
ndt_omp_ros2:   ${NDT_OMP_SHA}
EOF
cat .pinned_commit
```

Expected: file `.pinned_commit` contains two SHA lines.

- [ ] **Step 3: Convert to vendored directories (remove `.git`)**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws/src/lidarslam_ros2
rm -rf .git
rm -rf ndt_omp_ros2/.git
ls -la | head -20
```

Expected: no `.git` directory in either `lidarslam_ros2` or `lidarslam_ros2/ndt_omp_ros2`.

- [ ] **Step 4: Confirm ROS package names**

```bash
grep -h "<name>" src/lidarslam_ros2/*/package.xml src/lidarslam_ros2/*/*/package.xml 2>/dev/null | sort -u
```

Expected output contains at least: `<name>lidarslam</name>`, `<name>lidarslam_components</name>`, `<name>lidarslam_msgs</name>`, `<name>ndt_omp_ros2</name>` (exact wrappers may differ — record actual names for use in launch file).

If `lidarslam` package name differs (e.g. `scanmatcher` + `graph_based_slam`), record the actual name(s); use those wherever the plan says `lidarslam`.

- [ ] **Step 5: Locate launch files shipped by upstream**

```bash
find src/lidarslam_ros2 -name "*.launch.py" -o -name "*.launch.xml"
```

Expected: at least one launch file (typically `lidarslam.launch.py` under `lidarslam/launch/`). Record full path for use in bringup.

- [ ] **Step 6: Commit**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git add src/lidarslam_ros2/
git commit -m "$(cat <<'EOF'
feat(slam): vendor lidarslam_ros2 + ndt_omp_ros2

Clone rsasaki0109/lidarslam_ros2 with --recursive (pulls in
ndt_omp_ros2 submodule). Pin both SHAs in .pinned_commit. Remove
.git directories to flatten into vendored sources.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

Expected: commit succeeds. Many new files added (lidarslam + ndt_omp source trees).

---

## Task 3: Implement pose_relay Node

**Files:**
- Create: `src/go2_nav_bridge/src/pose_relay.cpp`
- Modify: `src/go2_nav_bridge/CMakeLists.txt`

`package.xml` already declares `tf2_ros` and `tf2_geometry_msgs` from the FAST-LIO2 restoration — no changes needed.

- [ ] **Step 1: Write pose_relay.cpp**

Create `src/go2_nav_bridge/src/pose_relay.cpp`:

```cpp
// pose_relay — converts /current_pose (PoseStamped) to a TF odom->base_link.
//
// lidarslam_ros2 publishes its frontend estimate as /current_pose with
// frame_id = "map" but the actual displacement carried by that pose is the
// robot's full SLAM-resolved motion. By republishing it as odom->base_link
// (and pairing it with the identity map->odom from map_odom_broadcaster) we
// reconstruct the REP-105 chain without modifying upstream lidarslam C++.
//
// No interpolation, no buffering: each PoseStamped is broadcast 1:1.

#include <memory>
#include <string>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_stamped.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <tf2_ros/transform_broadcaster.h>

class PoseRelay : public rclcpp::Node {
public:
  PoseRelay() : Node("pose_relay") {
    odom_frame_ = declare_parameter<std::string>("odom_frame", "odom");
    base_frame_ = declare_parameter<std::string>("base_frame", "base_link");

    broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

    pose_sub_ = create_subscription<geometry_msgs::msg::PoseStamped>(
      "/current_pose", 10,
      [this](const geometry_msgs::msg::PoseStamped::SharedPtr msg) {
        relay(*msg);
      });

    RCLCPP_INFO(get_logger(),
      "pose_relay active: /current_pose -> TF %s -> %s",
      odom_frame_.c_str(), base_frame_.c_str());
  }

private:
  void relay(const geometry_msgs::msg::PoseStamped & msg) {
    geometry_msgs::msg::TransformStamped t;
    t.header.stamp = msg.header.stamp;
    t.header.frame_id = odom_frame_;
    t.child_frame_id = base_frame_;
    t.transform.translation.x = msg.pose.position.x;
    t.transform.translation.y = msg.pose.position.y;
    t.transform.translation.z = msg.pose.position.z;
    t.transform.rotation = msg.pose.orientation;
    broadcaster_->sendTransform(t);
  }

  std::string odom_frame_;
  std::string base_frame_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> broadcaster_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr pose_sub_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<PoseRelay>());
  rclcpp::shutdown();
  return 0;
}
```

- [ ] **Step 2: Update CMakeLists.txt — register pose_relay executable**

Edit `src/go2_nav_bridge/CMakeLists.txt`. After the `map_odom_broadcaster` block (after its `ament_target_dependencies(...)` line), insert:

```cmake
add_executable(pose_relay src/pose_relay.cpp)
ament_target_dependencies(pose_relay
  rclcpp
  geometry_msgs
  tf2_ros
  tf2_geometry_msgs
)
```

Modify the `install(TARGETS ...)` block to add `pose_relay`:

```cmake
install(TARGETS
  bridge_node
  map_odom_broadcaster
  pose_relay
  DESTINATION lib/${PROJECT_NAME}
)
```

- [ ] **Step 3: Verify file edits with grep**

```bash
grep -n "pose_relay" /home/ale/Documents/Uni/Tesi/go2_ws/src/go2_nav_bridge/CMakeLists.txt
```

Expected: at least 3 matches (add_executable line, ament_target_dependencies line, install target line).

- [ ] **Step 4: Commit**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git add src/go2_nav_bridge/src/pose_relay.cpp \
        src/go2_nav_bridge/CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat(nav): add pose_relay C++ node

Subscribes to /current_pose (PoseStamped, frame: map) from
lidarslam_ros2 and publishes TF odom->base_link 1:1. Paired with
map_odom_broadcaster (identity map->odom) to reconstruct the REP-105
chain map->odom->base_link without modifying upstream lidarslam.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 4: Create lidarslam Config

**Files:**
- Create: `src/go2_nav_bridge/config/lidarslam/params.yaml`

- [ ] **Step 1: Write params.yaml**

Create `src/go2_nav_bridge/config/lidarslam/params.yaml`:

```yaml
# lidarslam_ros2 frontend (scan-matcher) + backend (graph-based-slam)
# parameters tuned for Unitree Go2 + Hesai XT16.
#
# Key choices:
# - NDT registration (stable across indoor/outdoor; GICP can be swapped via
#   registration_method)
# - ndt_resolution: 2.0 — smaller than upstream default 5.0 because the XT16
#   produces a sparser cloud than the 32/64-line sensors lidarslam was
#   originally tested on
# - use_imu: true — distortion correction from Go2 /utlidar/imu (9-axis)
# - publish_tf: false — pose_relay handles the TF; this avoids a duplicate
#   map->base_link edge that would conflict with map_odom_broadcaster

/**:
    ros__parameters:
        registration_method: "NDT"
        ndt_resolution: 2.0
        ndt_num_threads: 0
        gicp_corr_dist_threshold: 5.0
        trans_for_mapupdate: 1.0
        vg_size_for_input: 0.2
        vg_size_for_map: 0.05
        use_min_max_filter: true
        scan_max_range: 20.0
        scan_min_range: 0.5
        scan_period: 0.1
        map_publish_period: 15.0
        num_targeted_cloud: 10
        debug_flag: false
        set_initial_pose: false
        publish_tf: false
        use_odom: false
        use_imu: true
```

- [ ] **Step 2: Verify config file**

```bash
grep -E "(registration_method|use_imu|publish_tf|scan_period)" \
  /home/ale/Documents/Uni/Tesi/go2_ws/src/go2_nav_bridge/config/lidarslam/params.yaml
```

Expected output:
```
        registration_method: "NDT"
        scan_period: 0.1
        publish_tf: false
        use_imu: true
```

- [ ] **Step 3: Commit**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git add src/go2_nav_bridge/config/lidarslam/params.yaml
git commit -m "$(cat <<'EOF'
feat(slam): add lidarslam_ros2 params for Go2 + Hesai XT16

NDT registration (ndt_resolution 2.0 for sparse 16-line cloud),
use_imu enabled (Go2 /utlidar/imu, scan_period 0.1), publish_tf
disabled (pose_relay owns the TF edge).

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 5: Rewrite bringup.launch.py

**Files:**
- Modify: `src/go2_nav_bridge/launch/bringup.launch.py`
- Modify: `src/go2_nav_bridge/launch/octomap.launch.py` (topic remap)

- [ ] **Step 1: Replace bringup.launch.py contents**

Replace the entire contents of `src/go2_nav_bridge/launch/bringup.launch.py` with:

```python
# bringup.launch.py — lidarslam_ros2 experimental SLAM variant.
#
# REP-105 frame chain:
#   map ─[map_odom_broadcaster, dynamic identity 50 Hz]─> odom
#       ─[pose_relay, ~10 Hz, from /current_pose]──────> base_link
#       ─[static extrinsic T=(0.171, 0, 0.0908)]───────> hesai_lidar
#
# pose_relay closes the REP-105 gap left by lidarslam (which would
# otherwise emit map->base_link directly). publish_tf is forced to
# false in the lidarslam param file.
#
# Octomap is fed raw /lidar_points (not a registered cloud) because
# lidarslam has no per-scan registered output; TF lookups place
# points into map correctly via the chain above.

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


def _static_tf(parent: str, child: str, x='0', y='0', z='0',
               yaw='0', pitch='0', roll='0') -> ExecuteProcess:
    return ExecuteProcess(
        cmd=[
            'ros2', 'run', 'tf2_ros', 'static_transform_publisher',
            '--x', x, '--y', y, '--z', z,
            '--yaw', yaw, '--pitch', pitch, '--roll', roll,
            '--frame-id', parent, '--child-frame-id', child,
        ],
        output='screen',
    )


def generate_launch_description():
    hesai_dir = get_package_share_directory('hesai_ros_driver')
    lidarslam_dir = get_package_share_directory('lidarslam')
    go2_nav_bridge_dir = get_package_share_directory('go2_nav_bridge')

    params_file = os.path.join(
        go2_nav_bridge_dir, 'config', 'lidarslam', 'params.yaml')

    # map -> odom (identity now; upgradeable to loop closure injection).
    map_odom = Node(
        package='go2_nav_bridge',
        executable='map_odom_broadcaster',
        name='map_odom_broadcaster',
        output='screen',
        parameters=[{
            'publish_rate_hz': 50.0,
            'map_frame': 'map',
            'odom_frame': 'odom',
        }],
    )

    # odom -> base_link, driven by lidarslam's /current_pose.
    pose_relay = Node(
        package='go2_nav_bridge',
        executable='pose_relay',
        name='pose_relay',
        output='screen',
        parameters=[{
            'odom_frame': 'odom',
            'base_frame': 'base_link',
        }],
    )

    # base_link -> hesai_lidar (official Go2 Expansion Dock extrinsic).
    tf_base_link_hesai = _static_tf('base_link', 'hesai_lidar',
                                    x='0.171', y='0.0', z='0.0908')

    hesai_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(hesai_dir, 'launch', 'start.py')
        ),
    )

    # lidarslam: include upstream launch with our params + topic remaps.
    # Hesai publishes /lidar_points; lidarslam expects /input_cloud.
    # Go2 IMU is /utlidar/imu; lidarslam expects /imu.
    lidarslam_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(lidarslam_dir, 'launch', 'lidarslam.launch.py')
        ),
        launch_arguments={
            'scanmatcher_param_file': params_file,
            'graph_based_slam_param_file': params_file,
        }.items(),
    )

    nav2_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(go2_nav_bridge_dir, 'launch', 'nav2.launch.py')
        ),
    )

    octomap_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(go2_nav_bridge_dir, 'launch', 'octomap.launch.py')
        ),
    )

    bridge_node = Node(
        package='go2_nav_bridge',
        executable='bridge_node',
        name='go2_nav_bridge',
        output='screen',
        respawn=True,
        respawn_delay=1.0,
    )

    return LaunchDescription([
        map_odom,
        pose_relay,
        tf_base_link_hesai,
        hesai_launch,
        lidarslam_launch,
        nav2_launch,
        octomap_launch,
        bridge_node,
    ])
```

- [ ] **Step 2: Inspect upstream launch arguments**

The launch_arguments above (`scanmatcher_param_file`, `graph_based_slam_param_file`) assume the upstream launch file accepts those names. Verify:

```bash
grep -n "DeclareLaunchArgument\|param_file" \
  /home/ale/Documents/Uni/Tesi/go2_ws/src/lidarslam_ros2/lidarslam/launch/lidarslam.launch.py 2>/dev/null
```

Expected: argument names containing "param_file" or similar are declared.

If the argument names differ (e.g. `param_file` only, or `lidarslam_param_file`), adjust the `launch_arguments` dict in `bringup.launch.py` accordingly. Report the actual argument names used.

If topic remappings (`/input_cloud`, `/imu`) are NOT applied by the upstream launch with command-line `--remap`-style args, add an explicit relay solution: a second IncludeLaunchDescription wrapper or replace the IncludeLaunchDescription with direct `Node(...)` calls instantiating `scanmatcher` and `graph_based_slam` nodes with `remappings=[('input_cloud','/lidar_points'),('imu','/utlidar/imu')]`.

- [ ] **Step 3: Add topic remappings if upstream launch lacks them**

If Step 2 confirmed the upstream `lidarslam.launch.py` does NOT remap topics, REPLACE the `lidarslam_launch` block in `bringup.launch.py` with direct Node instantiations. Inspect upstream launch:

```bash
cat /home/ale/Documents/Uni/Tesi/go2_ws/src/lidarslam_ros2/lidarslam/launch/lidarslam.launch.py
```

Then in `bringup.launch.py`, replace the `lidarslam_launch` block with:

```python
    scanmatcher_node = Node(
        package='scanmatcher',
        executable='scanmatcher_node',
        name='scan_matcher',
        output='screen',
        parameters=[params_file],
        remappings=[
            ('input_cloud', '/lidar_points'),
            ('imu', '/utlidar/imu'),
        ],
    )

    graph_slam_node = Node(
        package='graph_based_slam',
        executable='graph_based_slam_node',
        name='graph_based_slam',
        output='screen',
        parameters=[params_file],
    )
```

And add both to the returned `LaunchDescription([...])`, replacing the `lidarslam_launch` entry.

Use the exact `package` and `executable` names observed in upstream (from Step 2's output) — these may differ slightly from `scanmatcher` / `graph_based_slam`.

- [ ] **Step 4: Update octomap.launch.py — point cloud source**

Edit `src/go2_nav_bridge/launch/octomap.launch.py`. Replace the `cloud_in` remap target:

```python
            remappings=[
                ('cloud_in', '/lidar_points'),
            ]
```

(Replaces the previous `/cloud_registered` mapping inherited from the FAST-LIO2 branch.)

Verify:

```bash
grep "cloud_in" /home/ale/Documents/Uni/Tesi/go2_ws/src/go2_nav_bridge/launch/octomap.launch.py
```

Expected: `('cloud_in', '/lidar_points'),`

- [ ] **Step 5: Commit**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git add src/go2_nav_bridge/launch/bringup.launch.py \
        src/go2_nav_bridge/launch/octomap.launch.py
git commit -m "$(cat <<'EOF'
feat(nav): switch bringup launch to lidarslam_ros2 + pose_relay

bringup.launch.py: launch lidarslam (scan-matcher + graph SLAM)
with params.yaml, pair with map_odom_broadcaster (map->odom) and
pose_relay (odom->base_link from /current_pose).

octomap.launch.py: feed raw /lidar_points (lidarslam has no
per-scan registered cloud equivalent of /cloud_registered).

Topic remaps: /lidar_points -> /input_cloud, /utlidar/imu -> /imu.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 6: Update Dockerfile

**Files:**
- Modify: `docker/Dockerfile`

- [ ] **Step 1: Read current builder stage apt block**

```bash
sed -n '11,40p' /home/ale/Documents/Uni/Tesi/go2_ws/docker/Dockerfile
```

Expected: apt block ends with the FAST-LIO2 additions (`libomp-dev`, `libpcl-dev`, `ros-humble-eigen-stl-containers`, `ros-humble-pcl-conversions`, `ros-humble-pcl-ros`).

- [ ] **Step 2: Remove FAST-LIO2-only apt packages from builder stage**

Open `docker/Dockerfile` with the Edit tool. Locate the builder-stage apt block. Remove these two lines (lidarslam_ros2 + ndt_omp_ros2 don't require them):

```
  ros-humble-eigen-stl-containers \
  ros-humble-pcl-conversions \
```

Keep `libomp-dev`, `libpcl-dev`, `ros-humble-pcl-ros` — needed by both FAST-LIO2 and lidarslam/ndt_omp.

If removing the lines leaves a trailing `\` on the line above the closing newline, fix the trailing backslash to land on the actual last package.

- [ ] **Step 3: Verify Dockerfile syntactic validity**

```bash
docker buildx build --check -f /home/ale/Documents/Uni/Tesi/go2_ws/docker/Dockerfile /home/ale/Documents/Uni/Tesi/go2_ws 2>&1 | tail -10
```

Expected: no syntax warnings related to dangling backslashes or trailing line continuations. If `--check` is unsupported on the local Docker version, fall back to:

```bash
grep -nE '\\\s*$' /home/ale/Documents/Uni/Tesi/go2_ws/docker/Dockerfile | head -20
```

Inspect the result; every `\` ending should have another package line immediately after it (no `\` followed by `RUN` or blank).

- [ ] **Step 4: Commit**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git add docker/Dockerfile
git commit -m "$(cat <<'EOF'
chore(docker): drop FAST-LIO2-only apt deps for lidarslam variant

Remove ros-humble-eigen-stl-containers and ros-humble-pcl-conversions
from the builder stage — required by FAST-LIO2 but not by
lidarslam_ros2 / ndt_omp_ros2. PCL, OpenMP and pcl-ros are retained
(used by both stacks).

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 7: Local Build (Optional — Skip if ROS Not Installed on Host)

**Files:** (none — verification only)

- [ ] **Step 1: Detect ROS 2 Humble on host**

```bash
test -f /opt/ros/humble/setup.bash && echo "ROS humble present" || echo "ROS not on host — skip to Task 8"
```

If "ROS not on host", proceed to Task 8.

- [ ] **Step 2: colcon build the new packages**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
source /opt/ros/humble/setup.bash
colcon build --symlink-install \
  --packages-select ndt_omp_ros2 lidarslam_msgs lidarslam_components lidarslam scanmatcher graph_based_slam go2_nav_bridge \
  2>&1 | tail -30
```

Expected: `Summary: N packages finished` with no `Failed` entries. If some package names from the `--packages-select` list don't exist in the upstream layout, run without `--packages-select` to build everything.

- [ ] **Step 3: Smoke-test pose_relay**

```bash
source /home/ale/Documents/Uni/Tesi/go2_ws/install/setup.bash
ros2 run go2_nav_bridge pose_relay &
RELAY_PID=$!
sleep 1
ros2 topic pub --once /current_pose geometry_msgs/msg/PoseStamped \
  "{header: {frame_id: 'map'}, pose: {position: {x: 1.0, y: 2.0, z: 0.0}, orientation: {w: 1.0}}}"
sleep 1
ros2 run tf2_ros tf2_echo odom base_link --timeout 2
kill $RELAY_PID
```

Expected: tf2_echo output contains `Translation: [1.000, 2.000, 0.000]`.

---

## Task 8: Docker ARM64 Build

**Files:** (none — build verification)

- [ ] **Step 1: Run the multi-stage Docker build**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
docker compose build go2_navigation 2>&1 | tail -40
```

Expected: build completes with no errors. The colcon step builds the new lidarslam packages alongside the rest of the workspace.

If a missing-dependency error appears for `lidarslam_msgs` or similar, run:

```bash
docker compose build --no-cache go2_navigation 2>&1 | tail -40
```

If errors persist, capture the failing apt or colcon line and stop — escalate to user.

---

## Task 9: On-Robot Deploy and Verify

**Files:** (none — deployment + runtime checks)

- [ ] **Step 1: Sync to robot**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
./sync_to_dog.sh 2>&1 | tail -20
```

Expected: rsync finishes with `total size is ...` line.

- [ ] **Step 2: Bring stack up on robot**

On the robot (over ssh to `unitree@192.168.123.18` or in-band):

```bash
cd <repo_path_on_robot>
docker compose down
docker compose up -d
docker compose logs -f --tail=80
```

Expected log lines:
- `map_odom_broadcaster active: map -> odom @ 50.0 Hz`
- `pose_relay active: /current_pose -> TF odom -> base_link`
- `Hesai LiDAR driver started`
- A line from lidarslam confirming the scan-matcher / graph-SLAM nodes started

Stop log tailing with Ctrl-C.

- [ ] **Step 3: Verify TF chain**

From an operator laptop on the same ROS_DOMAIN_ID:

```bash
ros2 run tf2_tools view_frames
xdg-open frames.pdf
```

Expected: `map → odom → base_link → hesai_lidar` chain visible. No dangling frames.

- [ ] **Step 4: Verify topic activity**

```bash
ros2 topic hz /current_pose --window 50
```

Expected: ≥ 5 Hz once the robot starts moving. If the rate is 0, lidarslam is not initializing — inspect logs.

```bash
ros2 topic hz /tf
```

Expected: ≥ 50 Hz (dominated by `map_odom_broadcaster`).

```bash
ros2 run tf2_ros tf2_echo map base_link --timeout 5
```

Expected: a pose. After walking the robot forward 1 m, the value should change by ~1 m on the appropriate axis.

- [ ] **Step 5: RViz visual check**

Open RViz with `Fixed Frame = map`. Add:
- `PointCloud2` on `/map` (lidarslam accumulated map; updates every ~15 s)
- `Path` on `/path`
- `PoseStamped` on `/current_pose`
- `Map` on `/projected_map` (octomap output)

Expected:
- `/map` cloud accumulates as the robot moves (walls/floor discernible)
- `/current_pose` arrow tracks the robot smoothly
- `/projected_map` populates with occupancy cells

If `/current_pose` jumps or `/path` shows discontinuities, capture a rosbag of `/lidar_points`, `/utlidar/imu`, `/current_pose`, and `/tf` for offline analysis before retuning.

- [ ] **Step 6: Nav2 goal test**

In RViz: place a `2D Goal Pose` ~1 m in front of the robot.

Expected: Nav2 plans a path; `/cmd_vel` publishes; `go2_nav_bridge` translates to `SportModeCmd`; robot moves.

- [ ] **Step 7: Commit deployment marker**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git commit --allow-empty -m "$(cat <<'EOF'
chore(slam): lidarslam_ros2 integration verified on-robot

End-to-end check complete:
- map_odom_broadcaster + pose_relay TF chain valid
- /current_pose published during motion
- octomap populates from /lidar_points
- Nav2 goal executes

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>
EOF
)"
```

---

## Task 10: Quick Comparison Log (Optional)

**Files:**
- Create: `docs/superpowers/notes/2026-05-21-lidarslam-vs-fastlio2.md`

- [ ] **Step 1: Record subjective comparison notes**

After running both stacks on the same trajectory, write a short qualitative comparison:

- Odometry drift (visual estimate after closed-loop traversal)
- Initialization latency
- CPU usage (`docker stats` or `top` inside container)
- Loop closure observed? (lidarslam graph SLAM backend)
- Stability of `/current_pose` rate

This is informal — the goal is to inform the future decision on whether to retain lidarslam.

- [ ] **Step 2: Commit notes**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git add -f docs/superpowers/notes/2026-05-21-lidarslam-vs-fastlio2.md
git commit -m "docs(slam): qualitative FAST-LIO2 vs lidarslam comparison notes

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Self-Review Notes

Spec coverage:
- ✓ Branch creation from `feature/fast-lio2-restore-rep105` (Task 1)
- ✓ Vendor lidarslam_ros2 + ndt_omp_ros2 (Task 2)
- ✓ `pose_relay` node (Task 3)
- ✓ `params.yaml` with NDT + IMU + `publish_tf: false` (Task 4)
- ✓ `bringup.launch.py` swap + topic remaps (Task 5)
- ✓ Octomap remap to `/lidar_points` (Task 5, Step 4)
- ✓ Dockerfile cleanup (Task 6)
- ✓ Build verification (Tasks 7, 8)
- ✓ On-robot verification per success criteria (Task 9)

Type consistency:
- `pose_relay` executable, `PoseRelay` class, `pose_relay` node name — consistent across `.cpp`, CMakeLists, launch
- `map_odom_broadcaster` reused unchanged from FAST-LIO2 branch
- Topic names: `/current_pose`, `/input_cloud`, `/imu`, `/lidar_points`, `/utlidar/imu` consistent across config, launch, and verification commands

Risks acknowledged:
- Upstream launch arg names may differ — Task 5 Steps 2-3 document discovery procedure
- Topic remap mechanism (launch_arguments vs direct Node remappings) may need swap — Task 5 Step 3
- Package names (`lidarslam` vs `scanmatcher`/`graph_based_slam`) may differ — Task 2 Step 4 records actual names
- IMU rate from `/utlidar/imu` unverified — if too low (<50 Hz), distortion correction may degrade; capture rate with `ros2 topic hz /utlidar/imu` during Task 9
