# FAST-LIO2 Restore Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Restore FAST-LIO2 SLAM pipeline on Go2 / Hesai XT16; add C++ `map_odom_broadcaster` node publishing dynamic identity transform (upgradeable to loop closure later).

**Architecture:** Revert GLIM migration. FAST-LIO2 publishes `camera_init → body` at IMU rate. Static identity bridges reconcile to REP-105 chain `map → odom → camera_init → body → base_link → hesai_lidar`. New C++ node replaces `static_transform_publisher` for `map → odom` (runtime broadcaster, subscribes to `/initialpose` stub for future relocalization).

**Tech Stack:** ROS 2 Humble, FAST-LIO2 (`hku-mars/FAST_LIO` ros2 branch), C++17, ament_cmake, tf2_ros, CycloneDDS, Docker (ARM64 cross), Hesai XT16 driver.

**Spec:** `docs/superpowers/specs/2026-05-21-fast-lio2-restore-design.md`

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `src/fast_lio_ros2/` | create (clone) | SLAM front-end |
| `src/fast_lio_ros2/config/hesai_xt16.yaml` | restore from git | Hesai XT16 sensor config |
| `src/go2_nav_bridge/src/map_odom_broadcaster.cpp` | create | Dynamic `map→odom` TF publisher |
| `src/go2_nav_bridge/CMakeLists.txt` | modify | Add new executable + tf2_ros dep |
| `src/go2_nav_bridge/package.xml` | modify | Add tf2_ros build/exec dep |
| `src/go2_nav_bridge/launch/bringup.launch.py` | rewrite | Replace GLIM with FAST-LIO2 + broadcaster |
| `src/go2_nav_bridge/config/glim/` | delete | Cleanup GLIM artifacts |
| `docker/Dockerfile` | modify | Remove GLIM PPA, add FAST-LIO2 deps |
| `docker/docker-compose.yml` | modify | Remove nvidia runtime |
| `config/imu.yaml` | retain | IMU noise params (Allan variance, valid for both SLAMs) |

---

## Task 1: Restore FAST-LIO2 Source Tree

**Files:**
- Create: `src/fast_lio_ros2/` (cloned)
- Create: `src/fast_lio_ros2/config/hesai_xt16.yaml` (from git history)

- [ ] **Step 1: Clone FAST-LIO2 ros2 branch into vendor location**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws/src
git clone -b ros2 https://github.com/hku-mars/FAST_LIO.git fast_lio_ros2
cd fast_lio_ros2
git submodule update --init --recursive
# Pin to current HEAD for reproducibility
FAST_LIO_SHA=$(git rev-parse HEAD)
echo "Pinned FAST-LIO2 to ${FAST_LIO_SHA}" > .pinned_commit
rm -rf .git  # Convert to vendored directory (no submodule)
cd ..
```

Expected: `src/fast_lio_ros2/` exists with `CMakeLists.txt`, `package.xml`, `src/laserMapping.cpp`, `config/`, `launch/`.

- [ ] **Step 2: Restore Hesai XT16 config from previous working commit**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git show df5776e:src/fast_lio_ros2/config/hesai_xt16.yaml > src/fast_lio_ros2/config/hesai_xt16.yaml
```

Expected: file exists with `lidar_type: 2`, `N_SCANS: 16`, `extrinsic_T: [0.171, 0.0, 0.0908]`, `extrinsic_R: [1,0,0, 0,1,0, 0,0,1]`.

- [ ] **Step 3: Verify config contents**

```bash
grep -E "(lidar_type|N_SCANS|extrinsic_T|extrinsic_R|gyr_cov|acc_cov)" /home/ale/Documents/Uni/Tesi/go2_ws/src/fast_lio_ros2/config/hesai_xt16.yaml
```

Expected output contains:
- `lidar_type: 2`
- `N_SCANS: 16`
- `extrinsic_T: [ 0.171, 0.0, 0.0908 ]`
- `extrinsic_R: [ 1, 0, 0, 0, 1, 0, 0, 0, 1 ]`

If any key is missing, abort and inspect git history of `df5776e:src/fast_lio_ros2/config/hesai_xt16.yaml` manually.

- [ ] **Step 4: Verify FAST-LIO2 launch file exists**

```bash
ls /home/ale/Documents/Uni/Tesi/go2_ws/src/fast_lio_ros2/launch/mapping.launch.py
```

Expected: file present (provided by upstream clone).

- [ ] **Step 5: Commit**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git add src/fast_lio_ros2/
git commit -m "feat(slam): restore FAST-LIO2 vendored directory

Clone hku-mars/FAST_LIO ros2 branch, pin to current HEAD (see
.pinned_commit). Restore hesai_xt16.yaml byte-identical to working
config from commit df5776e.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Task 2: Implement map_odom_broadcaster Node

**Files:**
- Create: `src/go2_nav_bridge/src/map_odom_broadcaster.cpp`
- Modify: `src/go2_nav_bridge/CMakeLists.txt`
- Modify: `src/go2_nav_bridge/package.xml`

- [ ] **Step 1: Add tf2_ros build/exec dependency in package.xml**

Edit `src/go2_nav_bridge/package.xml`. Insert after `<depend>geometry_msgs</depend>`:

```xml
  <depend>tf2_ros</depend>
  <depend>tf2_geometry_msgs</depend>
```

- [ ] **Step 2: Write map_odom_broadcaster.cpp**

Create `src/go2_nav_bridge/src/map_odom_broadcaster.cpp`:

```cpp
// map_odom_broadcaster — publishes a dynamic map→odom transform.
//
// Initial behavior: identity. Subscribes to /initialpose as a forward-
// compatibility hook so a future loop-closure / relocalization layer can
// update the published transform without touching the launch graph.
//
// REP-105: map and odom are world-fixed frames; map can jump on relocalization,
// odom is continuous. With FAST-LIO2 (no loop closure), map ≡ odom is the
// physically correct initial state.

#include <chrono>
#include <memory>

#include <rclcpp/rclcpp.hpp>
#include <geometry_msgs/msg/pose_with_covariance_stamped.hpp>
#include <geometry_msgs/msg/transform_stamped.hpp>
#include <tf2_ros/transform_broadcaster.h>

using namespace std::chrono_literals;

class MapOdomBroadcaster : public rclcpp::Node {
public:
  MapOdomBroadcaster() : Node("map_odom_broadcaster") {
    publish_rate_hz_ = declare_parameter<double>("publish_rate_hz", 50.0);
    map_frame_ = declare_parameter<std::string>("map_frame", "map");
    odom_frame_ = declare_parameter<std::string>("odom_frame", "odom");

    transform_.header.frame_id = map_frame_;
    transform_.child_frame_id = odom_frame_;
    transform_.transform.rotation.w = 1.0;  // identity

    broadcaster_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

    const auto period = std::chrono::duration<double>(1.0 / publish_rate_hz_);
    timer_ = create_wall_timer(
      std::chrono::duration_cast<std::chrono::nanoseconds>(period),
      [this]() { publish(); });

    initialpose_sub_ = create_subscription<geometry_msgs::msg::PoseWithCovarianceStamped>(
      "/initialpose", 10,
      [this](const geometry_msgs::msg::PoseWithCovarianceStamped::SharedPtr msg) {
        on_initialpose(*msg);
      });

    RCLCPP_INFO(get_logger(),
      "map_odom_broadcaster active: %s -> %s @ %.1f Hz",
      map_frame_.c_str(), odom_frame_.c_str(), publish_rate_hz_);
  }

private:
  void publish() {
    transform_.header.stamp = now();
    broadcaster_->sendTransform(transform_);
  }

  void on_initialpose(const geometry_msgs::msg::PoseWithCovarianceStamped & msg) {
    // Forward-compatibility stub: future loop closure / relocalization
    // layer publishes here to nudge map->odom. Currently a passive update.
    const auto & p = msg.pose.pose;
    transform_.transform.translation.x = p.position.x;
    transform_.transform.translation.y = p.position.y;
    transform_.transform.translation.z = p.position.z;
    transform_.transform.rotation = p.orientation;
    RCLCPP_INFO(get_logger(),
      "map->odom updated from /initialpose: t=(%.3f, %.3f, %.3f)",
      p.position.x, p.position.y, p.position.z);
  }

  double publish_rate_hz_;
  std::string map_frame_;
  std::string odom_frame_;
  geometry_msgs::msg::TransformStamped transform_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> broadcaster_;
  rclcpp::TimerBase::SharedPtr timer_;
  rclcpp::Subscription<geometry_msgs::msg::PoseWithCovarianceStamped>::SharedPtr initialpose_sub_;
};

int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  rclcpp::spin(std::make_shared<MapOdomBroadcaster>());
  rclcpp::shutdown();
  return 0;
}
```

- [ ] **Step 3: Update CMakeLists.txt — add tf2_ros find_package and new executable**

Edit `src/go2_nav_bridge/CMakeLists.txt`. After `find_package(unitree_go REQUIRED)` (line 20), insert:

```cmake
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)
```

After the `bridge_node` executable block (after the closing `)` of `ament_target_dependencies(bridge_node ...)`), insert:

```cmake
add_executable(map_odom_broadcaster src/map_odom_broadcaster.cpp)
ament_target_dependencies(map_odom_broadcaster
  rclcpp
  geometry_msgs
  tf2_ros
  tf2_geometry_msgs
)
```

Modify the existing `install(TARGETS ...)` block to include the new executable:

```cmake
install(TARGETS
  bridge_node
  map_odom_broadcaster
  DESTINATION lib/${PROJECT_NAME}
)
```

- [ ] **Step 4: Build to verify compilation**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
source /opt/ros/humble/setup.bash
colcon build --packages-select go2_nav_bridge --symlink-install 2>&1 | tail -20
```

Expected: `Finished <<< go2_nav_bridge` with no errors. If `tf2_ros not found`, install `ros-humble-tf2-ros` and `ros-humble-tf2-geometry-msgs` via apt.

- [ ] **Step 5: Smoke-test the node**

```bash
source install/setup.bash
ros2 run go2_nav_bridge map_odom_broadcaster &
sleep 2
ros2 topic hz /tf --window 50 &
TF_PID=$!
sleep 3
kill $TF_PID
pkill -f map_odom_broadcaster
```

Expected: `/tf` average rate ≈ 50 Hz.

Verify the transform content:

```bash
ros2 run go2_nav_bridge map_odom_broadcaster &
sleep 1
ros2 run tf2_ros tf2_echo map odom --timeout 2
pkill -f map_odom_broadcaster
```

Expected output contains:
```
- Translation: [0.000, 0.000, 0.000]
- Rotation: in Quaternion [0.000, 0.000, 0.000, 1.000]
```

- [ ] **Step 6: Commit**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git add src/go2_nav_bridge/src/map_odom_broadcaster.cpp \
        src/go2_nav_bridge/CMakeLists.txt \
        src/go2_nav_bridge/package.xml
git commit -m "feat(nav): add map_odom_broadcaster C++ node

Publishes dynamic map->odom identity transform at 50 Hz. Subscribes to
/initialpose as a forward-compatibility stub for future loop closure /
relocalization integration. Replaces the static_transform_publisher
previously used for map->odom.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Task 3: Rewrite bringup.launch.py

**Files:**
- Modify: `src/go2_nav_bridge/launch/bringup.launch.py`

- [ ] **Step 1: Replace the entire bringup.launch.py with the FAST-LIO2-based version**

Replace the contents of `src/go2_nav_bridge/launch/bringup.launch.py` with:

```python
# bringup.launch.py
#
# REP-105 frame chain:
#   map ─[map_odom_broadcaster, dynamic identity 50 Hz]─> odom
#       ─[static identity]─> camera_init
#       ─[FAST-LIO2 laserMapping ~100 Hz]─> body
#       ─[static identity]─> base_link
#       ─[static extrinsic T=(0.171, 0, 0.0908)]─> hesai_lidar
#
# Static identity bridges (odom->camera_init, body->base_link) reconcile
# FAST-LIO2's hardcoded frame names with REP-105 without patching upstream
# C++ source. Replace map_odom_broadcaster's identity with a real loop-closure
# corrector when relocalization is added — no other launch changes required.

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
    fast_lio_dir = get_package_share_directory('fast_lio')
    go2_nav_bridge_dir = get_package_share_directory('go2_nav_bridge')

    # Dynamic map -> odom (identity now; upgradeable to loop closure).
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

    # Static identity bridges to reconcile FAST-LIO2 hardcoded frames with REP-105.
    tf_odom_camera_init = _static_tf('odom', 'camera_init')
    tf_body_base_link = _static_tf('body', 'base_link')

    # base_link -> hesai_lidar — official Unitree Go2 Expansion Dock extrinsic
    # (T=[0.171, 0, 0.0908], R=I3). Mirrors FAST-LIO2 IMU->LiDAR extrinsic_T
    # in hesai_xt16.yaml (body ≡ base_link convention).
    tf_base_link_hesai = _static_tf('base_link', 'hesai_lidar',
                                    x='0.171', y='0.0', z='0.0908')

    hesai_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(hesai_dir, 'launch', 'start.py')
        ),
    )

    fast_lio_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(fast_lio_dir, 'launch', 'mapping.launch.py')
        ),
        launch_arguments={
            'config_file': 'hesai_xt16.yaml',
            'rviz': 'false',
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
        tf_odom_camera_init,
        tf_body_base_link,
        tf_base_link_hesai,
        hesai_launch,
        fast_lio_launch,
        nav2_launch,
        octomap_launch,
        bridge_node,
    ])
```

- [ ] **Step 2: Verify octomap.launch.py topic remapping still matches FAST-LIO2 output**

```bash
grep -n "cloud_in\|cloud_registered\|glim_ros" /home/ale/Documents/Uni/Tesi/go2_ws/src/go2_nav_bridge/launch/octomap.launch.py
```

If output contains `/glim_ros/cloud`, replace with `/cloud_registered`:

```bash
sed -i 's|/glim_ros/cloud|/cloud_registered|g' /home/ale/Documents/Uni/Tesi/go2_ws/src/go2_nav_bridge/launch/octomap.launch.py
```

If output already references `/cloud_registered`, no edit needed.

Re-verify:

```bash
grep "cloud_registered" /home/ale/Documents/Uni/Tesi/go2_ws/src/go2_nav_bridge/launch/octomap.launch.py
```

Expected: at least one match.

- [ ] **Step 3: Commit**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git add src/go2_nav_bridge/launch/bringup.launch.py \
        src/go2_nav_bridge/launch/octomap.launch.py
git commit -m "feat(nav): replace GLIM with FAST-LIO2 + map_odom_broadcaster

bringup.launch.py: revert to FAST-LIO2 launch; replace static map->odom
publisher with map_odom_broadcaster node (REP-105 chain via 2 identity
bridges + 1 extrinsic static TF).

octomap.launch.py: remap cloud_in from /glim_ros/cloud back to
/cloud_registered (FAST-LIO2 output topic).

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Task 4: Docker — Remove GLIM, Restore FAST-LIO2 Deps

**Files:**
- Modify: `docker/Dockerfile`
- Modify: `docker/docker-compose.yml`

- [ ] **Step 1: Inspect current GLIM blocks to identify exact line ranges**

```bash
grep -n "glim\|gtsam\|koide\|nvidia\|NVIDIA" /home/ale/Documents/Uni/Tesi/go2_ws/docker/Dockerfile /home/ale/Documents/Uni/Tesi/go2_ws/docker/docker-compose.yml
```

Expected output identifies two GLIM PPA blocks in Dockerfile (builder stage and runtime stage) and four nvidia-related lines in docker-compose.yml (two services × `runtime: nvidia` + env vars).

- [ ] **Step 2: Remove GLIM PPA block from builder stage (Dockerfile)**

Open `docker/Dockerfile`. Locate the comment line:
```
# GLIM GPU-accelerated SLAM (headers for colcon build; CUDA provided by NVIDIA runtime)
```
Delete this comment AND the entire `RUN curl -fsSL https://koide3.github.io/ppa/setup_ppa.sh ...` block that follows it (until the closing `&& rm -rf /var/lib/apt/lists/*` or similar). The block spans approximately 6 lines.

Use Edit tool with `old_string` being the exact GLIM block (read file first) and `new_string` empty.

- [ ] **Step 3: Remove GLIM PPA block from runtime stage (Dockerfile)**

Same procedure for the second occurrence near line 95.

- [ ] **Step 4: Add FAST-LIO2 apt dependencies (builder stage)**

In the builder stage, locate the main `apt-get install` block (the one that installs ROS dev tools). Add the following packages to its list, preserving alphabetical order where present:

```
      libomp-dev \
      libpcl-dev \
      ros-humble-eigen-stl-containers \
      ros-humble-pcl-conversions \
      ros-humble-pcl-ros \
```

If a separate `apt-get install` already provides any of these (e.g. `libpcl-dev` may be pulled by `ros-humble-pcl-ros`), do not duplicate — just ensure all five are present in the resolved dependency graph.

- [ ] **Step 5: Add FAST-LIO2 runtime apt dependencies (runtime stage)**

In the runtime stage, add to the main runtime `apt-get install`:

```
      libomp5 \
      libpcl-common1.12 \
      ros-humble-pcl-ros \
```

(Exact `libpcl-common` version suffix depends on the base image — adjust if `apt-get` reports the package as unfound; use `apt-cache search libpcl-common` inside the builder to find the correct suffix for Humble on the chosen base image.)

- [ ] **Step 6: Strip nvidia runtime from docker-compose.yml**

For each service that has it (`go2_navigation` and `dev`), remove these three lines:

```yaml
    runtime: nvidia
```
```yaml
      - NVIDIA_VISIBLE_DEVICES=all
      - NVIDIA_DRIVER_CAPABILITIES=compute,utility
```

Use Edit tool with `replace_all: true` and `old_string` set to one of the env var lines is unsafe (different services may have different indentation). Edit each occurrence by reading context first.

- [ ] **Step 7: Build container locally to verify Dockerfile correctness**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
docker compose build go2_navigation 2>&1 | tail -30
```

Expected: build completes successfully. If `ros-humble-pcl-ros` is not found, search for the correct package name with:
```bash
docker run --rm ros:humble bash -c "apt-get update && apt-cache search pcl | head"
```

- [ ] **Step 8: Commit**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git add docker/Dockerfile docker/docker-compose.yml
git commit -m "feat(docker): remove GLIM, restore FAST-LIO2 build deps

Drop koide3 PPA, ros-humble-glim-ros, libgtsam-points-dev from both
builder and runtime stages. Add libpcl-dev, libomp-dev, ros-humble-pcl-ros,
ros-humble-pcl-conversions, ros-humble-eigen-stl-containers for FAST-LIO2
compilation and runtime.

docker-compose.yml: drop runtime: nvidia and NVIDIA_* env vars from both
services. FAST-LIO2 is CPU-only — GPU runtime no longer required.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Task 5: Cleanup GLIM Artifacts

**Files:**
- Delete: `src/go2_nav_bridge/config/glim/` (entire directory)

- [ ] **Step 1: Confirm no other launch/source references the GLIM config path**

```bash
grep -rn "config/glim\|glim_ros\|glim_rosnode" /home/ale/Documents/Uni/Tesi/go2_ws/src/ /home/ale/Documents/Uni/Tesi/go2_ws/docker/ 2>/dev/null
```

Expected: only matches in `docs/`, `SESSION_DIARY.md`, or comments. No active launch/CMake reference. If any active reference remains, abort and fix it before deleting the directory.

- [ ] **Step 2: Delete the GLIM config directory**

```bash
rm -rf /home/ale/Documents/Uni/Tesi/go2_ws/src/go2_nav_bridge/config/glim/
```

- [ ] **Step 3: Verify CMakeLists.txt still installs config directory (catches all remaining configs under config/)**

```bash
grep -A3 "install(DIRECTORY" /home/ale/Documents/Uni/Tesi/go2_ws/src/go2_nav_bridge/CMakeLists.txt
```

Expected: `config` is listed in an `install(DIRECTORY ...)` block — install pattern unchanged, just fewer files inside.

- [ ] **Step 4: Commit**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git add -A src/go2_nav_bridge/config/
git commit -m "chore(slam): remove GLIM config artifacts

Cleanup config/glim/ directory after FAST-LIO2 restoration. All other
GLIM references already removed in earlier commits in this branch.

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Task 6: Full Build + On-Robot Deploy + Verify

**Files:** (no source changes — verification only)

- [ ] **Step 1: Full local colcon build**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
source /opt/ros/humble/setup.bash
colcon build --symlink-install 2>&1 | tail -40
```

Expected: `Summary: N packages finished` with no `Failed` entries.

If `fast_lio` build fails on missing OpenMP / PCL on host, that is expected for the local check — the authoritative build target is the Docker ARM64 image. Proceed.

- [ ] **Step 2: Docker ARM64 build for deployment**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
docker compose build go2_navigation 2>&1 | tail -30
```

Expected: `Successfully built <hash>` or equivalent buildx success message.

- [ ] **Step 3: Sync to robot**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
./sync_to_dog.sh 2>&1 | tail -20
```

Expected: rsync output ends with `sent X bytes` / `total size`.

- [ ] **Step 4: Bring stack up on robot**

On the robot (via ssh `unitree@192.168.123.18` or in-band):

```bash
cd <repo_path_on_robot>
docker compose down
docker compose up -d
docker compose logs -f --tail=50
```

Expected: `bringup.launch.py` starts. Look for log lines:
- `map_odom_broadcaster active: map -> odom @ 50.0 Hz`
- `FAST_LIO_MAPPING_NODE INIT`
- `Hesai LiDAR driver started`
- `Nav2 lifecycle nodes activated`

Stop log following with Ctrl-C.

- [ ] **Step 5: Verify TF chain from operator laptop**

```bash
ros2 run tf2_tools view_frames
xdg-open frames.pdf
```

Expected chain: `map → odom → camera_init → body → base_link → hesai_lidar` (and `base_link → utlidar_imu` if Unitree SDK is publishing it — orthogonal to nav stack).

- [ ] **Step 6: Verify TF rates**

```bash
ros2 topic hz /tf
```

Expected: combined rate ≥ 100 Hz (50 Hz from `map_odom_broadcaster` + ~100 Hz from FAST-LIO2 `camera_init → body`).

```bash
ros2 run tf2_ros tf2_echo map base_link --timeout 5
```

Expected: a numeric pose printed (non-zero if robot has moved; near zero if stationary).

- [ ] **Step 7: RViz visual sanity check**

Open RViz with `Fixed Frame = map`. Add displays:
- `PointCloud2` on `/cloud_registered`
- `Odometry` on `/Odometry`
- `Map` on `/projected_map` (octomap output)

Expected:
- Point cloud accumulates as a geometrically coherent representation of the environment (walls, floor, obstacles distinguishable).
- Single odometry arrow tracks the robot (no fan, no scatter).
- Projected map populates with occupancy cells matching point cloud structure.

If point cloud is scattered or odometry fans out, FAST-LIO2 is failing to initialize — capture logs and stop the deployment for diagnosis.

- [ ] **Step 8: Send a Nav2 goal**

In RViz: click `2D Pose Estimate` and place at robot's current location. Click `2D Goal Pose` and place ~1 m in front of the robot.

Expected: Nav2 plans a path (line from current pose to goal), MPPI controller publishes `/cmd_vel`, `go2_nav_bridge` translates to `SportModeCmd`, robot moves toward goal.

Stop with `2D Pose Estimate` set to current robot pose (cancels current goal in BT navigator).

- [ ] **Step 9: Commit a deployment marker**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git commit --allow-empty -m "chore(slam): FAST-LIO2 restoration verified on-robot

End-to-end verification complete:
- map_odom_broadcaster publishing at 50 Hz
- FAST-LIO2 camera_init->body at ~100 Hz
- TF chain map->odom->camera_init->body->base_link->hesai_lidar intact
- Point cloud, odometry visually stable in RViz
- Nav2 goal executes; bridge translates cmd_vel to SportModeCmd

Co-Authored-By: Claude Sonnet 4.6 <noreply@anthropic.com>"
```

---

## Post-Implementation

- [ ] **Update CLAUDE.md SLAM Core reference**

CLAUDE.md currently lists `src/fast_lio_ros2/src/laserMapping.cpp` as SLAM Core and `src/fast_lio_ros2/config/hesai_xt16.yaml` as Config God Node. After this restoration both paths are valid again — no change needed unless the post-restoration FAST-LIO2 clone uses a different path layout (verify in Task 1, Step 1).

- [ ] **Rename branch (optional)**

```bash
cd /home/ale/Documents/Uni/Tesi/go2_ws
git branch -m feature/glim-migration feature/fast-lio2-restore-rep105
```

(Branch name is now misleading; rename improves git log readability.)

---

## Self-Review Notes

Spec coverage:
- ✓ FAST-LIO2 source restore (Task 1)
- ✓ hesai_xt16.yaml restored from `df5776e` (Task 1, Step 2)
- ✓ map_odom_broadcaster (Task 2)
- ✓ bringup.launch.py rewrite with bridges (Task 3)
- ✓ Dockerfile GLIM removal + FAST-LIO2 deps (Task 4)
- ✓ docker-compose nvidia runtime removal (Task 4, Step 6)
- ✓ GLIM config cleanup (Task 5)
- ✓ End-to-end verification (Task 6)

Type consistency: `MapOdomBroadcaster` class, `map_odom_broadcaster` executable target,
`map_odom_broadcaster` node name — consistent across `.cpp`, CMakeLists, and launch file.

Risks acknowledged:
- FAST-LIO2 upstream may have diverged from `df5776e` working state — Task 1 pins SHA.
- `libpcl-common` version suffix may shift — Task 4, Step 5 documents discovery procedure.
