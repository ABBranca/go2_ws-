# Project: Unitree Go2 Autonomous Navigation

## Session Initialization
**MANDATORY:** Always invoke the following tools in parallel as the FIRST ACTION of every session:
1. `activate_skill(name="caveman")`
2. `activate_skill(name="graphify")`

Autonomous navigation for Unitree Go2 using Hesai XT16 LiDAR, FAST-LIO2 SLAM, and Nav2.

## 🚀 Workflows
- **Production:** `./sync_to_dog.sh` && `docker compose up --build -d` (Multi-stage ARM64 build).
- **Development:** `docker compose --profile dev up -d` (Volume mounts for rapid iteration).
- **Local:** VS Code Dev Containers with ROS 2 extension.

## 🏗️ Architecture
- **Pipeline:** Hesai Driver → FAST-LIO2 (SLAM) → Nav2 (Planning) → `go2_nav_bridge` (Control).
- **Control:** `cmd_vel` → `SportModeCmd` (high-level API).
- **TF Tree:** `map` → `odom` → `base_link` → `hesai_lidar` (Static T: [0.171, 0, 0.0908], R: I₃).
- **Networking:** Orin Dock (`192.168.123.18`) runs stack; MCU (`.161`) handles motion. USB Wi-Fi on Dock for telemetry.

## 🛠️ Key Components (via Graphify)
- **Control Hub:** `src/go2_nav_bridge/` (Hardened bridge with watchdogs/sanitization).
- **SLAM Core:** `src/fast_lio_ros2/src/laserMapping.cpp`.
- **Config God Nodes:** `src/fast_lio_ros2/config/hesai_xt16.yaml` (Extrinsics/IMU), `nav2_params.yaml`.
- **Deployment:** `docker/docker-compose.yml`.

## 📏 Standards & Rules
- **Safety:** Bridge must sanitize inputs (NaN/Inf) and include a velocity watchdog.
- **DDS:** Use `cyclonedds.xml` with explicit `eth0` interface to prevent discovery failure.
- **Subagents:** Parallel agents are read-only; apply all file writes in the main session.
- **Navigation:** **Always use `graphify`** (read `graphify-out/GRAPH_REPORT.md` or `graphify-out/wiki/`) to identify architectural hubs and dependencies before making changes.

## 🦴 Caveman Mode (Token Optimization)
Caveman Mode is active/available. Use it to reduce token usage and increase speed.
- **Activate:** "caveman mode" or `/caveman [lite|full|ultra]`.
- **Deactivate:** "stop caveman" or "normal mode".
- **Rules:** Drop articles/filler/pleasantries. Fragments OK. Technical terms stay.
- **Commands:** `/caveman-commit` (terse commits), `/caveman-review` (one-line review).
