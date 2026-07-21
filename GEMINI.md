# Project: Unitree Go2 Autonomous Navigation

## Session Initialization
**MANDATORY:** Always invoke the following tools in parallel as the FIRST ACTION of every session:
1. `activate_skill(name="caveman")`
2. `activate_skill(name="graphify")`

Autonomous navigation for Unitree Go2 using Hesai XT16 LiDAR, slam_toolbox (2D SLAM), and Nav2.

## 🚀 Workflows
- **Production:** `./sync_to_dog.sh` && `docker compose up --build -d` (Multi-stage ARM64 build).
- **Development:** `docker compose --profile dev up -d` (Volume mounts for rapid iteration).
- **Local:** VS Code Dev Containers with ROS 2 extension.

## 🏗️ Architecture
- **Pipeline:** Hesai Driver → pointcloud_to_laserscan (cloud→scan) → slam_toolbox (2D SLAM) → Nav2 (Planning) → `go2_nav_bridge` (Control).
- **Odometry:** planar leg odometry from the Go2 (`/utlidar/robot_odom`) via `odom_tf_broadcaster`; a horizontal XT16 cannot observe Z, so 3D LiDAR-inertial SLAM was retired.
- **Control:** `cmd_vel` → `SportModeCmd` (high-level API).
- **TF Tree:** `map` →(slam_toolbox)→ `odom` →(odom_tf_broadcaster)→ `base_link` →(static)→ `hesai_lidar` (Static T: [0.171, 0, 0.0908], R: I₃).
- **Networking:** Orin Dock (`192.168.123.18`) runs stack; MCU (`.161`) handles motion. USB Wi-Fi on Dock for telemetry.

## 🛠️ Key Components (via Graphify)
- **Control Hub:** `src/go2_nav_bridge/` (Hardened bridge with watchdogs/sanitization).
- **SLAM Core:** `slam_toolbox` (external apt pkg); orchestrated by `src/go2_nav_bridge/launch/bringup.launch.py`.
- **Config God Nodes:** `src/go2_nav_bridge/config/slam_toolbox_2d.yaml` (2D SLAM), `pointcloud_to_laserscan.yaml` (cloud→scan slab), `nav2_params.yaml`.
- **Deployment:** `docker/docker-compose.yml`.

## 📚 LLM Wiki
- **Vault:** `docs/wiki/pages/` (open as Obsidian vault)
- **Schema:** `docs/wiki/SCHEMA.md` — LLM behavior instructions
- **Sources:** drop docs in `docs/wiki/raw/`, then `ingest [filename]`
- **Skill:** `/wiki` — ingest / query / lint operations
- **Index:** `docs/wiki/pages/_index.md`

## 📏 Standards & Rules
- **Safety:** Bridge must sanitize inputs (NaN/Inf) and include a velocity watchdog.
- **DDS:** Use `cyclonedds.xml` with explicit `eth0` interface to prevent discovery failure.
- **Subagents:** Parallel agents are read-only; apply all file writes in the main session.
- **Navigation:** **Always use `graphify`** (read `graphify-out/GRAPH_REPORT.md` or `graphify-out/wiki/`) to identify architectural hubs and dependencies before making changes.
- **Documentation**: Always ask the user whether to fetch info about the documentation using the NotebookLM-MCP or not. **NEVER** act without knowing the most recent documentation about the task.

## 🦴 Caveman Mode (Token Optimization)
Caveman Mode is active/available. Use it to reduce token usage and increase speed.
- **Activate:** "caveman mode" or `/caveman [lite|full|ultra]`.
- **Deactivate:** "stop caveman" or "normal mode".
- **Rules:** Drop articles/filler/pleasantries. Fragments OK. Technical terms stay.
- **Commands:** `/caveman-commit` (terse commits), `/caveman-review` (one-line review).
