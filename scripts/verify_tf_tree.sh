#!/usr/bin/env bash
# verify_tf_tree.sh — pre-flight diagnostic for the Go2 2D mapping/nav pipeline.
#
# Run inside the running container (after sourcing the workspace) while
# bringup.launch.py / mapping_2d.launch.py is up. Verifies:
#   1. TF chain resolves end-to-end (REP-105, fully 2D):
#        map →(slam_toolbox)→ odom →(odom_tf_broadcaster)→ base_link →(static)→ hesai_lidar
#      plus base_link → base_stabilized (IMU-levelled frame the scan is projected into).
#   2. Required topics are flowing at expected frequencies.
#   3. /scan (pointcloud_to_laserscan) and /map (slam_toolbox) are alive.
#
# Exits 0 only if all checks pass.

set -u

ok()   { printf '\033[32m[OK]  \033[0m %s\n' "$1"; }
fail() { printf '\033[31m[FAIL]\033[0m %s\n' "$1"; FAILED=1; }
warn() { printf '\033[33m[WARN]\033[0m %s\n' "$1"; }

FAILED=0

if ! command -v ros2 >/dev/null; then
    fail "ros2 CLI not on PATH — source /opt/ros/humble/setup.bash and the workspace install/setup.bash"
    exit 2
fi

# 1. TF lookups (5 s window each).
# tf2_echo in ROS 2 Humble takes positional args only: source target [rate_hz].
# Passing --timeout is invalid (stod throws on the string), crashing the node.
# Instead: capture all output (ROS 2 RCLCPP_INFO goes to stderr) and check
# whether a "Translation:" line was printed — that only happens on a successful
# lookup.
check_tf() {
    local parent="$1" child="$2"
    local out
    out=$(timeout 5 ros2 run tf2_ros tf2_echo "$parent" "$child" 2>&1 || true)
    if printf '%s\n' "$out" | grep -q "Translation:"; then
        ok "TF $parent → $child resolves"
    else
        fail "TF $parent → $child NOT resolvable"
    fi
}
check_tf map           odom              # published by slam_toolbox
check_tf odom          base_link         # published by odom_tf_broadcaster (leg odom)
check_tf base_link     base_stabilized   # IMU-levelled scan frame
check_tf base_link     hesai_lidar       # static extrinsic
check_tf map           base_link         # full chain, the one the costmap uses

# 2. High-rate topic checks. Expected: lidar ~10 Hz, scan ~10 Hz, Go2 IMU ~200 Hz.
check_hz() {
    local topic="$1" min="$2"
    local rate
    rate=$(timeout 6 ros2 topic hz "$topic" 2>&1 | \
        grep -oE 'average rate: [0-9.]+' | head -1 | awk '{print $3}')
    if [[ -z "$rate" ]]; then
        fail "$topic — no messages received in 6 s"
        return
    fi
    awk -v r="$rate" -v m="$min" 'BEGIN{exit !(r >= m)}' && \
        ok "$topic @ ${rate} Hz (≥ ${min})" || \
        fail "$topic @ ${rate} Hz (< ${min})"
}
check_hz /lidar_points       8
check_hz /scan               5     # pointcloud_to_laserscan output
check_hz /utlidar/imu        100   # Go2 body IMU → odom_tf_broadcaster tilt source

# 3. Presence checks for variable-rate / latched topics (one message within timeout).
check_present() {
    local topic="$1"
    if timeout 8 ros2 topic echo --once "$topic" >/dev/null 2>&1; then
        ok "$topic — message received"
    else
        fail "$topic — no message within 8 s"
    fi
}
check_present /utlidar/robot_odom   # Go2 leg odometry (odom_tf_broadcaster input)
check_present /map                  # slam_toolbox occupancy grid (latched, low rate)

# 4. Frame_id sanity on /lidar_points (must be hesai_lidar).
fid=$(timeout 3 ros2 topic echo --once --field header.frame_id /lidar_points 2>/dev/null)
if [[ "$fid" == "hesai_lidar" ]]; then
    ok "/lidar_points frame_id = hesai_lidar"
else
    fail "/lidar_points frame_id = '$fid' (expected hesai_lidar)"
fi

if (( FAILED == 0 )); then
    printf '\n\033[32mAll checks passed — 2D pipeline ready.\033[0m\n'
    exit 0
else
    printf '\n\033[31mPipeline NOT ready — fix the issues above before running.\033[0m\n'
    exit 1
fi
