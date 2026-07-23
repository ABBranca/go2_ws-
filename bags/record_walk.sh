#!/usr/bin/env bash
# =============================================================================
# record_walk.sh — record a 2D-SLAM / tilt-leveling bag ON THE ORIN.
#
# WHY on the Orin (not the laptop): the Hesai PointCloud2 (/lidar_points) loses
# ~96% of clouds over the Wi-Fi link. Record at the source. On the Orin, the
# Hesai driver (/lidar_points) and the MCU (/utlidar/imu, /utlidar/robot_odom)
# already share Domain 0 — one `ros2 bag record` captures them together, no bridge.
#
# USAGE (inside the running production container):
#     docker exec -it go2_navigation bash /ros2_ws/bags/record_walk.sh [name]
#   or, from a shell already inside the container:
#     bash /ros2_ws/bags/record_walk.sh myrun
#   Ctrl-C to stop. Output persists on the host at ./bags/<name> (bind mount).
#
# ⚠ CLOCK-DOMAIN CAVEAT: /lidar_points is stamped with the Orin clock (often
# unsynced), /utlidar/imu and /utlidar/robot_odom with the MCU clock (UTC) —
# they can differ by ~56 years. Before 2D-SLAM replay, either sync the Orin
# clock (NTP/PTP) BEFORE recording, or re-stamp afterwards with
# `python3 bags/restamp_bag.py`. See mapping_2d_replay.launch.py.
# =============================================================================
set -euo pipefail

# ROS 2 env (idempotent — safe even if already sourced by the entrypoint)
source /opt/ros/humble/setup.bash
[ -f /ros2_ws/install/setup.bash ] && source /ros2_ws/install/setup.bash

# Must match the stack so discovery sees both the Hesai driver and the MCU.
export RMW_IMPLEMENTATION="${RMW_IMPLEMENTATION:-rmw_cyclonedds_cpp}"
export CYCLONEDDS_URI="${CYCLONEDDS_URI:-file:///cyclonedds.xml}"
export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-0}"

NAME="${1:-walk_$(date +%Y%m%d_%H%M%S)}"
OUT="/ros2_ws/bags/${NAME}"

# Topics for 2D SLAM + IMU tilt-leveling (see mapping_2d_replay.launch.py).
TOPICS=(
  /lidar_points          # Hesai XT16 PointCloud2 (Orin clock)
  /utlidar/imu           # MCU IMU ~200 Hz (tilt-leveling; MCU clock)
  /utlidar/robot_odom    # Go2 leg odometry (MCU clock)
  /tf
  /tf_static
)

echo "──────────────────────────────────────────────────────────"
echo " Recording bag → ${OUT}"
echo " Topics: ${TOPICS[*]}"
echo " Stop with Ctrl-C.  Reminder: check clock domains before replay."
echo "──────────────────────────────────────────────────────────"
exec ros2 bag record -o "${OUT}" "${TOPICS[@]}"
