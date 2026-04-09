# Design: Single-Command Startup (`docker compose up -d`)

**Date:** 2026-04-09
**Status:** Approved
**Author:** Alessandro Biagio Branca

---

## Goal

Replace the current manual 5-step launch sequence with a single command:

```bash
docker compose up -d
```

This must start the full navigation stack: Hesai XT16 driver → static TF → FAST-LIO2 SLAM → Nav2 → go2_nav_bridge.

---

## Approach

**Approach 1 selected:** Single `bringup.launch.py` master launch file inside `go2_nav_bridge`, combined with a multi-stage Dockerfile that builds the workspace at image build time.

---

## Architecture

### Startup Flow

```
docker compose up -d
    └─ Container start → ros_entrypoint.sh → source /ros2_ws/install/setup.bash
        └─ ros2 launch go2_nav_bridge bringup.launch.py
            ├─ static_transform_publisher  (base_link → hesai_lidar, T=[0.171,0,0.0908], R=I₃)
            ├─ hesai_ros_driver_2          (start.py, respawn=True)
            ├─ fast_lio mapping.launch.py  (config_file:=hesai_xt16.yaml, rviz:=false)
            ├─ go2_nav_bridge nav2.launch.py
            └─ go2_nav_bridge bridge_node  (respawn=True)
```

### Docker Build Strategy

Multi-stage Dockerfile:

- **Stage 1 (builder):** Copies `src/`, runs `rosdep install`, runs `colcon build`.
- **Stage 2 (runtime):** Copies only `install/` from builder. No build toolchain, no source code in the final image.
- **Result:** Lean production image; `docker compose up -d` starts in seconds.

### Docker Compose Profiles

| Profile | Command | Purpose |
|:---|:---|:---|
| *(default, none)* | `docker compose up -d` | Production: uses `CMD` from Dockerfile, launches full stack |
| `dev` | `docker compose --profile dev up -d` | Development: mounts `..:/ros2_ws` volume, opens bash shell |

The `dev` profile preserves the current interactive workflow for active development.

---

## Files to Modify

### `docker/Dockerfile`

**Change:** Restructure into multi-stage build.

```
Stage 1 (builder):
  FROM ros:humble-ros-base AS builder
  - Install build deps (same as current)
  - COPY src/ /ros2_ws/src/
  - RUN rosdep install --from-paths src --ignore-src -r -y
  - RUN colcon build --symlink-install \
        --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble

Stage 2 (runtime):
  FROM ros:humble-ros-base
  - Install runtime deps only (no build-essential, cmake, colcon, rosdep)
  - COPY --from=builder /ros2_ws/install /ros2_ws/install
  - COPY ros_entrypoint.sh /ros_entrypoint.sh
  - ENTRYPOINT ["/ros_entrypoint.sh"]
  - CMD ["ros2", "launch", "go2_nav_bridge", "bringup.launch.py"]
```

### `docker/docker-compose.yml`

**Change:**
- Remove `command: bash` from default service (CMD from Dockerfile takes over).
- Add `go2_navigation_dev` service under `profiles: ["dev"]` with volume mount and `command: bash`.

---

## Files to Create

### `src/go2_nav_bridge/launch/bringup.launch.py`

Master launch file. Launches nodes in this order:

1. `tf2_ros static_transform_publisher` — publishes `base_link → hesai_lidar` on `/tf_static` using named-argument syntax (Humble requirement).
2. `hesai_ros_driver_2/start.py` — LiDAR driver; `respawn=True`, `respawn_delay=2.0`.
3. `fast_lio/mapping.launch.py` — SLAM; `config_file:=hesai_xt16.yaml`, `rviz:=false`.
4. `go2_nav_bridge/nav2.launch.py` — Nav2 stack.
5. `go2_nav_bridge bridge_node` — velocity bridge; `respawn=True`, `respawn_delay=1.0`.

**Key implementation notes:**
- Static TF uses `ExecuteProcess` (not a Node action) since `static_transform_publisher` requires named args in Humble.
- FAST-LIO2 and Nav2 are included via `IncludeLaunchDescription` + `PythonLaunchDescriptionSource`.
- `rviz:=false` is hardcoded for robot-side deployment (visualization is remote via laptop).

### `src/go2_nav_bridge/launch/nav2.launch.py`

Includes `nav2_bringup/launch/navigation_launch.py` with:
- `params_file` pointing to `go2_nav_bridge/config/nav2_params.yaml`
- `use_sim_time:=false`

Lifecycle manager `node_names` **excludes** `map_server` and `amcl` — FAST-LIO2 publishes the `map → odom → base_link` TF chain directly.

### `src/go2_nav_bridge/config/nav2_params.yaml`

Full Nav2 parameter file configured for Go2 quadruped kinematics:

| Parameter | Value | Rationale |
|:---|:---|:---|
| Planner | `SmacPlannerHybrid` | Required for non-holonomic robots; supports kinematic constraints |
| Controller | `MPPI` | Optimal for quadruped dynamics; replaces default DWB |
| `minimum_turning_radius` | `0.35` m | Go2 estimated turning radius (0.3–0.5 m range) |
| Robot footprint | `[[-0.30, -0.15], [-0.30, 0.15], [0.30, 0.15], [0.30, -0.15]]` | Approximate Go2 body (0.6 × 0.3 m) |
| `max_vel_x` | `1.0` m/s | Matches `go2_nav_bridge` linear clamp |
| `max_vel_theta` | `1.0` rad/s | Matches `go2_nav_bridge` angular clamp |
| Lifecycle `node_names` | `[controller_server, planner_server, behavior_server, bt_navigator]` | Excludes map_server and amcl |

---

## Error Handling

- **LiDAR driver crash:** `respawn=True` with 2 s delay restarts the node automatically.
- **Bridge crash:** `respawn=True` with 1 s delay; watchdog (200 ms) publishes zero velocity during downtime, preventing runaway motion.
- **FAST-LIO2 divergence:** Not auto-restarted (restart without a good initial pose would corrupt the map). Operator must `docker compose restart`.
- **Nav2 lifecycle failure:** Nav2 lifecycle manager handles internal recovery; container restart as last resort.

---

## Constraints & Non-Goals

- **Non-goal:** `colcon build` at container runtime. Build is exclusively image-time.
- **Non-goal:** Rviz2 inside the container. Remote visualization via `rviz2` on the developer laptop (`ROS_DOMAIN_ID=1`).
- **Non-goal:** Automatic map saving/loading. FAST-LIO2 builds the map live from sensor data.
- **Constraint:** `install/setup.bash` must be sourced before launch. `ros_entrypoint.sh` handles this.
- **Constraint:** `cyclonedds.xml` must be mounted at `/cyclonedds.xml` with `CYCLONEDDS_URI=file:///cyclonedds.xml`. Already configured in `docker-compose.yml`.

---

## Deployment Workflow (post-implementation)

```bash
# On developer laptop — after code changes:
./sync_to_dog.sh
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose build && docker compose up -d"

# For active development (interactive shell with volume mount):
ssh unitree@192.168.123.18 "cd ~/go2_ws/docker && docker compose --profile dev up -d"
docker exec -it go2_navigation_dev bash
```

---

## Open Questions (deferred to implementation)

1. `hesai_ros_driver_2/start.py` currently also launches Rviz2 — needs to be forked or the `start.py` updated to accept a `rviz:=false` argument.
2. Nav2 `nav2_params.yaml` MPPI tuning values (weights, time horizon) are initial estimates — require field testing.
3. Go2 footprint polygon is approximate — verify against URDF or physical measurement.
