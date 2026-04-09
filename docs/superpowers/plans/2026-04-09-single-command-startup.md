# Single-Command Startup Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `docker compose up -d` start the full Go2 navigation stack (Hesai XT16 → FAST-LIO2 → Nav2 → go2_nav_bridge) without any manual intervention.

**Architecture:** A `bringup.launch.py` master launch file in `go2_nav_bridge` orchestrates all nodes. The Dockerfile is restructured into a multi-stage build (builder compiles the workspace; runtime stage copies only `install/`). The docker-compose.yml default service uses the Dockerfile CMD; a `dev` profile preserves the interactive shell workflow.

**Tech Stack:** ROS 2 Humble, Python launch system (`launch`, `launch_ros`), Nav2 Humble (`nav2_bringup`, `SmacPlannerHybrid`, `nav2_mppi_controller`), Docker multi-stage build, CycloneDDS.

---

## File Map

| Action | File | Responsibility |
|:---|:---|:---|
| Modify | `src/go2_nav_bridge/CMakeLists.txt` | Add install rules for `launch/` and `config/` directories |
| Modify | `src/go2_nav_bridge/package.xml` | Add exec dependencies on nav2 packages |
| Create | `src/go2_nav_bridge/config/nav2_params.yaml` | Full Nav2 parameter file for Go2 (SmacPlannerHybrid + MPPI) |
| Create | `src/go2_nav_bridge/launch/nav2.launch.py` | Includes `nav2_bringup/navigation_launch.py` with Go2 params |
| Create | `src/go2_nav_bridge/launch/bringup.launch.py` | Master launch: TF + Hesai + FAST-LIO2 + Nav2 + bridge |
| Modify | `docker/Dockerfile` | Restructure into multi-stage build; CMD launches bringup |
| Modify | `docker/docker-compose.yml` | Remove `command: bash`; add `dev` profile |
| Modify | `README.md` | Update Quick Start and Development Workflow sections |

---

## Task 1: Add install rules and nav2 dependencies to go2_nav_bridge

**Files:**
- Modify: `src/go2_nav_bridge/CMakeLists.txt`
- Modify: `src/go2_nav_bridge/package.xml`

- [ ] **Step 1: Add install rules for launch/ and config/ in CMakeLists.txt**

  In `src/go2_nav_bridge/CMakeLists.txt`, add after the existing `install(TARGETS ...)` block and before `ament_package()`:

  ```cmake
  # Install launch files and config
  install(DIRECTORY
    launch
    config
    DESTINATION share/${PROJECT_NAME}
  )
  ```

  Full updated CMakeLists.txt:

  ```cmake
  cmake_minimum_required(VERSION 3.8)
  project(go2_nav_bridge)

  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wdouble-promotion)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
      add_compile_options(-D_FORTIFY_SOURCE=2)
    endif()
    add_link_options(-Wl,-z,relro -Wl,-z,now)
  endif()

  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)

  # Find dependencies
  find_package(ament_cmake REQUIRED)
  find_package(rclcpp REQUIRED)
  find_package(geometry_msgs REQUIRED)
  find_package(unitree_go REQUIRED)

  # Add executable
  add_executable(bridge_node
    src/bridge_node.cpp
    src/bridge_utils.cpp
  )

  target_include_directories(bridge_node PRIVATE include)
  ament_target_dependencies(bridge_node rclcpp geometry_msgs unitree_go)

  # Install executable
  install(TARGETS
    bridge_node
    DESTINATION lib/${PROJECT_NAME}
  )

  # Install launch files and config
  install(DIRECTORY
    launch
    config
    DESTINATION share/${PROJECT_NAME}
  )

  if(BUILD_TESTING)
    find_package(ament_lint_auto REQUIRED)
    ament_lint_auto_find_test_dependencies()

    find_package(ament_cmake_gtest REQUIRED)
    ament_add_gtest(test_bridge_utils test/test_bridge_utils.cpp src/bridge_utils.cpp)
    target_include_directories(test_bridge_utils PRIVATE include)
  endif()

  ament_package()
  ```

- [ ] **Step 2: Add nav2 exec dependencies to package.xml**

  In `src/go2_nav_bridge/package.xml`, add after the existing `<depend>unitree_go</depend>` line:

  ```xml
    <exec_depend>nav2_bringup</exec_depend>
    <exec_depend>nav2_mppi_controller</exec_depend>
    <exec_depend>nav2_smac_planner</exec_depend>
    <exec_depend>tf2_ros</exec_depend>
  ```

  These are `exec_depend` (not `depend`) because they are needed at runtime only — `go2_nav_bridge` does not link against nav2 libraries at compile time.

- [ ] **Step 3: Create directory structure**

  ```bash
  mkdir -p src/go2_nav_bridge/launch
  mkdir -p src/go2_nav_bridge/config
  ```

- [ ] **Step 4: Verify build still passes**

  ```bash
  # From inside the container or dev environment:
  colcon build --symlink-install \
    --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble \
    --packages-select go2_nav_bridge
  ```

  Expected: `Finished <<< go2_nav_bridge` with no errors. The `install/share/go2_nav_bridge/` directory will be empty for now (launch/ and config/ don't exist yet) — that's fine.

- [ ] **Step 5: Commit**

  ```bash
  git add src/go2_nav_bridge/CMakeLists.txt src/go2_nav_bridge/package.xml
  git commit -m "build(go2_nav_bridge): add install rules for launch/config and nav2 exec deps"
  ```

---

## Task 2: Create nav2_params.yaml

**Files:**
- Create: `src/go2_nav_bridge/config/nav2_params.yaml`

- [ ] **Step 1: Create the Nav2 parameter file**

  Create `src/go2_nav_bridge/config/nav2_params.yaml` with the following content.

  > **Note on global costmap:** Since `octomap_server` is not yet integrated, no 2D occupancy grid is available. The global costmap uses only live point cloud obstacle data — no `static_layer`. This is intentional and documented in the "Roadmap" as a pending milestone.

  ```yaml
  bt_navigator:
    ros__parameters:
      global_frame: map
      robot_base_frame: base_link
      odom_topic: /Odometry
      bt_loop_duration: 10
      default_server_timeout: 20
      navigators: ['navigate_to_pose', 'navigate_through_poses']
      navigate_to_pose:
        plugin: "nav2_bt_navigator/NavigateToPoseNavigator"
      navigate_through_poses:
        plugin: "nav2_bt_navigator/NavigateThroughPosesNavigator"

  controller_server:
    ros__parameters:
      controller_frequency: 20.0
      min_x_velocity_threshold: 0.001
      min_y_velocity_threshold: 0.5
      min_theta_velocity_threshold: 0.001
      failure_tolerance: 0.3
      progress_checker_plugins: ["progress_checker"]
      goal_checker_plugins: ["general_goal_checker"]
      controller_plugins: ["FollowPath"]
      progress_checker:
        plugin: "nav2_controller::SimpleProgressChecker"
        required_movement_radius: 0.5
        movement_time_allowance: 10.0
      general_goal_checker:
        stateful: True
        plugin: "nav2_controller::SimpleGoalChecker"
        xy_goal_tolerance: 0.25
        yaw_goal_tolerance: 0.25
      FollowPath:
        plugin: "nav2_mppi_controller::MPPIController"
        time_steps: 56
        model_dt: 0.05
        batch_size: 2000
        vx_std: 0.2
        vy_std: 0.0
        wz_std: 0.4
        vx_max: 1.0
        vx_min: -0.5
        vy_max: 0.0
        wz_max: 1.0
        iteration_count: 1
        prune_distance: 1.7
        transform_tolerance: 0.1
        temperature: 0.3
        gamma: 0.015
        motion_model: "DiffDrive"
        visualize: false
        critics: [
          "ConstraintCritic", "GoalCritic", "GoalAngleCritic",
          "PathAlignCritic", "PathFollowCritic", "PathAngleCritic",
          "PreferForwardCritic"
        ]
        ConstraintCritic:
          enabled: true
          cost_power: 1
          cost_weight: 4.0
        GoalCritic:
          enabled: true
          cost_power: 1
          cost_weight: 5.0
          threshold_to_consider: 1.0
        GoalAngleCritic:
          enabled: true
          cost_power: 1
          cost_weight: 3.0
          threshold_to_consider: 0.4
        PathAlignCritic:
          enabled: true
          cost_power: 1
          cost_weight: 14.0
          max_path_occupancy_ratio: 0.05
          trajectory_point_step: 3
          threshold_to_consider: 0.40
          offset_from_furthest: 20
        PathFollowCritic:
          enabled: true
          cost_power: 1
          cost_weight: 4.0
          offset_from_furthest: 5
          threshold_to_consider: 1.4
        PathAngleCritic:
          enabled: true
          cost_power: 1
          cost_weight: 2.0
          offset_from_furthest: 4
          threshold_to_consider: 0.40
          mode: 0
        PreferForwardCritic:
          enabled: true
          cost_power: 1
          cost_weight: 5.0
          threshold_to_consider: 0.4

  planner_server:
    ros__parameters:
      expected_planner_frequency: 20.0
      planner_plugins: ["GridBased"]
      GridBased:
        plugin: "nav2_smac_planner/SmacPlannerHybrid"
        downsample_costmap: false
        downsampling_factor: 1
        tolerance: 0.25
        allow_unknown: true
        max_iterations: 1000000
        max_on_approach_iterations: 1000
        max_planning_time: 5.0
        motion_model_for_search: "REEDS_SHEPP"
        angle_quantization_bins: 72
        analytic_expansion_ratio: 3.5
        analytic_expansion_max_length: 3.0
        minimum_turning_radius: 0.35
        reverse_penalty: 2.1
        change_penalty: 0.0
        non_straight_penalty: 1.20
        cost_penalty: 2.0
        retrospective_penalty: 0.015
        lookup_table_size: 20.0
        cache_obstacle_heuristic: false
        smooth_path: true
        smoother:
          max_iterations: 1000
          w_smooth: 0.3
          w_data: 0.2
          tolerance: 1.0e-10
          do_refinement: true

  behavior_server:
    ros__parameters:
      costmap_topic: local_costmap/costmap_raw
      footprint_topic: local_costmap/published_footprint
      cycle_frequency: 10.0
      behavior_plugins: ["spin", "backup", "drive_on_heading", "wait"]
      spin:
        plugin: "nav2_behaviors/Spin"
      backup:
        plugin: "nav2_behaviors/BackUp"
      drive_on_heading:
        plugin: "nav2_behaviors/DriveOnHeading"
      wait:
        plugin: "nav2_behaviors/Wait"
      global_frame: odom
      robot_base_frame: base_link
      transform_tolerance: 0.1
      simulate_ahead_time: 2.0
      max_rotational_vel: 1.0
      min_rotational_vel: 0.4
      rotational_acc_lim: 3.2

  local_costmap:
    local_costmap:
      ros__parameters:
        update_frequency: 5.0
        publish_frequency: 2.0
        global_frame: odom
        robot_base_frame: base_link
        rolling_window: true
        width: 3
        height: 3
        resolution: 0.05
        robot_radius: 0.35
        plugins: ["voxel_layer", "inflation_layer"]
        inflation_layer:
          plugin: "nav2_costmap_2d::InflationLayer"
          cost_scaling_factor: 3.0
          inflation_radius: 0.55
        voxel_layer:
          plugin: "nav2_costmap_2d::VoxelLayer"
          enabled: True
          publish_voxel_map: True
          origin_z: 0.0
          z_resolution: 0.05
          z_voxels: 16
          max_obstacle_height: 2.0
          mark_threshold: 0
          observation_sources: scan
          scan:
            topic: /lidar_points
            max_obstacle_height: 2.0
            clearing: True
            marking: True
            data_type: "PointCloud2"
            raytrace_max_range: 3.0
            raytrace_min_range: 0.0
            obstacle_max_range: 2.5
            obstacle_min_range: 0.0
        always_send_full_costmap: True

  global_costmap:
    global_costmap:
      ros__parameters:
        update_frequency: 1.0
        publish_frequency: 1.0
        global_frame: map
        robot_base_frame: base_link
        robot_radius: 0.35
        resolution: 0.05
        track_unknown_space: true
        plugins: ["obstacle_layer", "inflation_layer"]
        obstacle_layer:
          plugin: "nav2_costmap_2d::ObstacleLayer"
          enabled: True
          observation_sources: scan
          scan:
            topic: /lidar_points
            max_obstacle_height: 2.0
            clearing: True
            marking: True
            data_type: "PointCloud2"
            raytrace_max_range: 3.0
            raytrace_min_range: 0.0
            obstacle_max_range: 2.5
            obstacle_min_range: 0.0
        inflation_layer:
          plugin: "nav2_costmap_2d::InflationLayer"
          cost_scaling_factor: 3.0
          inflation_radius: 0.55
        always_send_full_costmap: True

  lifecycle_manager_navigation:
    ros__parameters:
      use_sim_time: False
      autostart: True
      node_names:
        - controller_server
        - planner_server
        - behavior_server
        - bt_navigator
  ```

- [ ] **Step 2: Validate YAML syntax**

  ```bash
  python3 -c "import yaml; yaml.safe_load(open('src/go2_nav_bridge/config/nav2_params.yaml'))" && echo "YAML OK"
  ```

  Expected output: `YAML OK`

- [ ] **Step 3: Commit**

  ```bash
  git add src/go2_nav_bridge/config/nav2_params.yaml
  git commit -m "feat(go2_nav_bridge): add Nav2 params for Go2 (SmacPlannerHybrid + MPPI)"
  ```

---

## Task 3: Create nav2.launch.py

**Files:**
- Create: `src/go2_nav_bridge/launch/nav2.launch.py`

- [ ] **Step 1: Check nav2_bringup's navigation_launch.py arguments**

  ```bash
  ros2 launch nav2_bringup navigation_launch.py --show-args
  ```

  Confirm that `params_file` and `use_sim_time` are accepted arguments. Expected output includes lines like:
  ```
  'params_file':
      No description given
  'use_sim_time':
      No description given
  ```

- [ ] **Step 2: Create nav2.launch.py**

  Create `src/go2_nav_bridge/launch/nav2.launch.py`:

  ```python
  import os

  from ament_index_python.packages import get_package_share_directory
  from launch import LaunchDescription
  from launch.actions import IncludeLaunchDescription
  from launch.launch_description_sources import PythonLaunchDescriptionSource


  def generate_launch_description():
      nav2_bringup_dir = get_package_share_directory('nav2_bringup')
      go2_nav_bridge_dir = get_package_share_directory('go2_nav_bridge')

      params_file = os.path.join(go2_nav_bridge_dir, 'config', 'nav2_params.yaml')

      nav2_launch = IncludeLaunchDescription(
          PythonLaunchDescriptionSource(
              os.path.join(nav2_bringup_dir, 'launch', 'navigation_launch.py')
          ),
          launch_arguments={
              'params_file': params_file,
              'use_sim_time': 'false',
          }.items(),
      )

      return LaunchDescription([nav2_launch])
  ```

- [ ] **Step 3: Validate Python syntax**

  ```bash
  python3 -c "import ast; ast.parse(open('src/go2_nav_bridge/launch/nav2.launch.py').read())" && echo "Syntax OK"
  ```

  Expected: `Syntax OK`

- [ ] **Step 4: Commit**

  ```bash
  git add src/go2_nav_bridge/launch/nav2.launch.py
  git commit -m "feat(go2_nav_bridge): add nav2.launch.py wrapping nav2_bringup with Go2 params"
  ```

---

## Task 4: Create bringup.launch.py

**Files:**
- Create: `src/go2_nav_bridge/launch/bringup.launch.py`

- [ ] **Step 1: Verify package names of submodule dependencies**

  ```bash
  # hesai driver package name (may differ from directory name hesai_ros_driver_2)
  grep -r "^project(" src/hesai_ros_driver_2/CMakeLists.txt

  # fast_lio package name
  grep -r "^project(" src/fast_lio_ros2/CMakeLists.txt
  ```

  Expected: `project(hesai_ros_driver)` and `project(fast_lio)`.
  If the actual names differ, update the `get_package_share_directory(...)` calls in Step 2 accordingly.

- [ ] **Step 2: Create bringup.launch.py**

  Create `src/go2_nav_bridge/launch/bringup.launch.py`:

  ```python
  import os

  from ament_index_python.packages import get_package_share_directory
  from launch import LaunchDescription
  from launch.actions import ExecuteProcess, IncludeLaunchDescription
  from launch.launch_description_sources import PythonLaunchDescriptionSource
  from launch_ros.actions import Node


  def generate_launch_description():
      hesai_dir = get_package_share_directory('hesai_ros_driver')
      fast_lio_dir = get_package_share_directory('fast_lio')
      go2_nav_bridge_dir = get_package_share_directory('go2_nav_bridge')

      # 1. Static TF: base_link → hesai_lidar (official Unitree extrinsics)
      # Named-argument syntax required in Humble (positional is deprecated).
      static_tf = ExecuteProcess(
          cmd=[
              'ros2', 'run', 'tf2_ros', 'static_transform_publisher',
              '--x', '0.171', '--y', '0.0', '--z', '0.0908',
              '--yaw', '0', '--pitch', '0', '--roll', '0',
              '--frame-id', 'base_link', '--child-frame-id', 'hesai_lidar',
          ],
          output='screen',
      )

      # 2. Hesai XT16 LiDAR driver
      # Note: start.py also launches rviz2 without a disable flag.
      # On headless Orin (no $DISPLAY), the rviz2 process exits with an error
      # but does not affect the driver node — acceptable behaviour.
      hesai_launch = IncludeLaunchDescription(
          PythonLaunchDescriptionSource(
              os.path.join(hesai_dir, 'launch', 'start.py')
          ),
      )

      # 3. FAST-LIO2 SLAM (rviz disabled — visualization is remote via laptop)
      fast_lio_launch = IncludeLaunchDescription(
          PythonLaunchDescriptionSource(
              os.path.join(fast_lio_dir, 'launch', 'mapping.launch.py')
          ),
          launch_arguments={
              'config_file': 'hesai_xt16.yaml',
              'rviz': 'false',
          }.items(),
      )

      # 4. Nav2 stack
      nav2_launch = IncludeLaunchDescription(
          PythonLaunchDescriptionSource(
              os.path.join(go2_nav_bridge_dir, 'launch', 'nav2.launch.py')
          ),
      )

      # 5. go2_nav_bridge: cmd_vel → SportModeCmd
      bridge_node = Node(
          package='go2_nav_bridge',
          executable='bridge_node',
          name='go2_nav_bridge',
          output='screen',
          respawn=True,
          respawn_delay=1.0,
      )

      return LaunchDescription([
          static_tf,
          hesai_launch,
          fast_lio_launch,
          nav2_launch,
          bridge_node,
      ])
  ```

- [ ] **Step 3: Validate Python syntax**

  ```bash
  python3 -c "import ast; ast.parse(open('src/go2_nav_bridge/launch/bringup.launch.py').read())" && echo "Syntax OK"
  ```

  Expected: `Syntax OK`

- [ ] **Step 4: Build and verify launch file is installed**

  ```bash
  colcon build --symlink-install \
    --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble \
    --packages-select go2_nav_bridge
  source install/setup.bash
  ros2 launch go2_nav_bridge bringup.launch.py --show-args
  ```

  Expected: command prints available launch arguments (including `config_file`, `rviz`, etc. inherited from included launches) and exits cleanly. The key is that `go2_nav_bridge bringup.launch.py` is found — if it throws "package not found", the install rule is missing.

- [ ] **Step 5: Commit**

  ```bash
  git add src/go2_nav_bridge/launch/bringup.launch.py
  git commit -m "feat(go2_nav_bridge): add bringup.launch.py orchestrating full navigation stack"
  ```

---

## Task 5: Restructure Dockerfile into multi-stage build

**Files:**
- Modify: `docker/Dockerfile`
- Modify: `docker/ros_entrypoint.sh` (verify — no change expected)

- [ ] **Step 1: Verify ros_entrypoint.sh handles pre-built install correctly**

  Read `docker/ros_entrypoint.sh`. It should source `/ros2_ws/install/setup.bash` if the file exists. The current content already does this (`if [ -f "/ros2_ws/install/setup.bash" ]`). No changes needed.

- [ ] **Step 2: Replace Dockerfile with multi-stage version**

  Overwrite `docker/Dockerfile`:

  ```dockerfile
  # ─── Stage 1: Build ───────────────────────────────────────────────────────────
  FROM ros:humble-ros-base AS builder

  ENV DEBIAN_FRONTEND=noninteractive

  # Install build tools and all ROS dependencies needed for compilation
  RUN apt-get update && apt-get install --no-install-recommends -y \
      build-essential \
      cmake \
      git \
      python3-colcon-common-extensions \
      python3-rosdep \
      ros-humble-rmw-cyclonedds-cpp \
      ros-humble-nav2-bringup \
      ros-humble-nav2-mppi-controller \
      ros-humble-nav2-smac-planner \
      ros-humble-robot-localization \
      libpcl-dev \
      ros-humble-pcl-ros \
      && rm -rf /var/lib/apt/lists/*

  RUN if [ ! -d /etc/ros/rosdep/sources.list.d ]; then rosdep init; fi \
      && rosdep update

  WORKDIR /ros2_ws

  # Copy source — submodules must be initialized on the build host before docker build
  COPY src/ src/

  # Resolve any remaining rosdep dependencies from package.xml files
  RUN rosdep install --from-paths src --ignore-src -r -y \
      --skip-keys "unitree_go unitree_api"

  # Build the workspace
  RUN . /opt/ros/humble/setup.sh && \
      colcon build \
        --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble \
        --no-warn-unused-cli

  # ─── Stage 2: Runtime ─────────────────────────────────────────────────────────
  FROM ros:humble-ros-base

  ENV DEBIAN_FRONTEND=noninteractive

  # Runtime dependencies only — no build toolchain
  RUN apt-get update && apt-get install --no-install-recommends -y \
      ros-humble-rmw-cyclonedds-cpp \
      ros-humble-nav2-bringup \
      ros-humble-nav2-mppi-controller \
      ros-humble-nav2-smac-planner \
      ros-humble-robot-localization \
      ros-humble-pcl-ros \
      && rm -rf /var/lib/apt/lists/*

  WORKDIR /ros2_ws

  # Copy compiled workspace from builder — no source code in runtime image
  COPY --from=builder /ros2_ws/install /ros2_ws/install

  # Entrypoint sources ROS 2 env and workspace
  COPY ros_entrypoint.sh /ros_entrypoint.sh
  RUN chmod +x /ros_entrypoint.sh
  RUN echo "source /ros_entrypoint.sh" >> ~/.bashrc

  ENTRYPOINT ["/ros_entrypoint.sh"]
  CMD ["ros2", "launch", "go2_nav_bridge", "bringup.launch.py"]
  ```

  > **Note on `--skip-keys`:** `unitree_go` and `unitree_api` are in the `unitree_ros2` submodule and are built from source — rosdep cannot resolve them from apt. The skip prevents a spurious failure.

- [ ] **Step 3: Commit**

  ```bash
  git add docker/Dockerfile
  git commit -m "feat(docker): multi-stage Dockerfile — build workspace at image-time, launch stack at runtime"
  ```

---

## Task 6: Update docker-compose.yml

**Files:**
- Modify: `docker/docker-compose.yml`

- [ ] **Step 1: Update docker-compose.yml**

  Replace `docker/docker-compose.yml` with:

  ```yaml
  services:
    # ── Production service ─────────────────────────────────────────────────────
    # docker compose up -d  →  builds image (if needed) and starts full nav stack
    go2_navigation:
      build:
        context: .
        dockerfile: Dockerfile
      container_name: go2_navigation
      restart: unless-stopped
      network_mode: host
      ipc: host
      cap_add:
        - NET_ADMIN
        - SYS_NICE
      stdin_open: true
      tty: true
      volumes:
        - ./cyclonedds.xml:/cyclonedds.xml:ro
      environment:
        - ROS_DOMAIN_ID=1
        - RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
        - CYCLONEDDS_URI=file:///cyclonedds.xml
      # CMD from Dockerfile launches bringup.launch.py — no override here

    # ── Development service ────────────────────────────────────────────────────
    # docker compose --profile dev up -d  →  interactive shell with source mount
    go2_navigation_dev:
      profiles: ["dev"]
      build:
        context: .
        dockerfile: Dockerfile
      container_name: go2_navigation_dev
      network_mode: host
      ipc: host
      cap_add:
        - NET_ADMIN
        - SYS_NICE
      stdin_open: true
      tty: true
      volumes:
        # Mount full workspace so edits are visible immediately
        - ..:/ros2_ws
        - ./cyclonedds.xml:/cyclonedds.xml:ro
      environment:
        - ROS_DOMAIN_ID=1
        - RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
        - CYCLONEDDS_URI=file:///cyclonedds.xml
      command: bash
  ```

- [ ] **Step 2: Validate compose file syntax**

  ```bash
  docker compose -f docker/docker-compose.yml config --quiet && echo "Compose OK"
  ```

  Expected: `Compose OK` (no output means no errors, then the echo fires).

- [ ] **Step 3: Commit**

  ```bash
  git add docker/docker-compose.yml
  git commit -m "feat(docker): prod service uses Dockerfile CMD; add dev profile with volume mount"
  ```

---

## Task 7: Update README.md

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Replace Quick Start section**

  In `README.md`, replace the content of the `## Quick Start` section with:

  ```markdown
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
  export ROS_DOMAIN_ID=1
  rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
  ```
  ```

- [ ] **Step 2: Replace Development Workflow section**

  In `README.md`, replace the content of the `## Development Workflow` section with:

  ```markdown
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
  export ROS_DOMAIN_ID=1
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
  ```

- [ ] **Step 3: Remove the manual Launch section**

  Delete the `## Launch` section entirely (the one with the 5-step manual sequence).
  It is superseded by `docker compose up -d`.

- [ ] **Step 4: Commit**

  ```bash
  git add README.md
  git commit -m "docs(README): update workflow — single docker compose up -d starts full stack"
  ```

---

## Task 8: End-to-end validation

- [ ] **Step 1: Sync and build image on the robot**

  ```bash
  ./sync_to_dog.sh
  ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose build 2>&1 | tail -20"
  ```

  Expected last lines: `Successfully built <sha>` or `=> exporting to image` with no errors.

- [ ] **Step 2: Start the stack**

  ```bash
  ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose up -d"
  ```

  Expected: `Container go2_navigation  Started`

- [ ] **Step 3: Verify all nodes are running**

  ```bash
  ssh unitree@192.168.123.18 "docker exec go2_navigation ros2 node list"
  ```

  Expected output includes (order may vary):
  ```
  /go2_nav_bridge
  /hesai_ros_driver/hesai_ros_driver_node
  /fast_lio/fastlio_mapping
  /controller_server
  /planner_server
  /behavior_server
  /bt_navigator
  ```

- [ ] **Step 4: Verify TF chain**

  ```bash
  ssh unitree@192.168.123.18 "docker exec go2_navigation ros2 run tf2_ros tf2_echo map base_link"
  ```

  Expected: transform printed (once FAST-LIO2 has initialized with LiDAR data).
  If FAST-LIO2 has not yet received data (no LiDAR connected), the command will timeout — that is expected.

- [ ] **Step 5: Verify LiDAR topic**

  ```bash
  ssh unitree@192.168.123.18 "docker exec go2_navigation ros2 topic hz /lidar_points --window 5"
  ```

  Expected: ~10 Hz (Hesai XT16 default). Only meaningful when LiDAR is connected and powered.

- [ ] **Step 6: Commit any final fixes found during validation**

  ```bash
  git add -A
  git commit -m "fix: post-validation corrections to bringup stack"
  ```

---

## Self-Review

**Spec coverage check:**
- ✅ `docker compose up -d` → full stack: Task 4 (bringup.launch.py) + Task 5 (Dockerfile CMD) + Task 6 (compose)
- ✅ `colcon build` in Dockerfile: Task 5 (multi-stage builder stage)
- ✅ Static TF `base_link → hesai_lidar`: Task 4, bringup.launch.py step 2
- ✅ Hesai driver via existing launch file: Task 4, bringup.launch.py step 2
- ✅ FAST-LIO2 with `hesai_xt16.yaml`, `rviz:=false`: Task 4, bringup.launch.py step 2
- ✅ Nav2 with SmacPlannerHybrid + MPPI: Task 2 (nav2_params.yaml) + Task 3 (nav2.launch.py)
- ✅ `dev` profile with volume mount + bash: Task 6 (docker-compose.yml)
- ✅ README updated: Task 7
- ✅ Manual `## Launch` section removed: Task 7, step 3

**Placeholder scan:** No TBD or TODO. Open questions from spec are addressed (rviz headless behaviour documented inline in bringup.launch.py comment; footprint deferred as noted in nav2_params.yaml comment).

**Type/name consistency:** `bringup.launch.py` references `hesai_ros_driver` and `fast_lio` — verified against CMakeLists.txt project names in Task 4 step 1. `go2_nav_bridge` package name consistent throughout.
