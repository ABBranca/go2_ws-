#!/usr/bin/env bash
# Offline 2D-SLAM replay validation for bag slam2d_19700101_102247.
# Starts mapping_2d_replay launch, plays the bag with --clock, saves the map,
# and reports occupancy-grid stats. Run inside the humble_ws distrobox.
WS=/home/ale/Documents/Uni/Tesi/go2_ws
BAG=${1:-$WS/bags/slam2d_19700101_102247}
OUT=${2:-$WS/bags/slam2d_test_out}
export ROS_DOMAIN_ID=77            # isolate from any other ROS graph
export ROS_LOCALHOST_ONLY=1

source /opt/ros/humble/setup.bash
source $WS/src/install/setup.bash
rm -rf "$OUT"; mkdir -p "$OUT"

echo "=== launch SLAM pipeline (bg) ==="
ros2 launch go2_nav_bridge mapping_2d_replay.launch.py > "$OUT/launch.log" 2>&1 &
LPID=$!
sleep 8                            # let slam_toolbox / pc2scan / odom_tf come up

echo "=== play bag (--clock, ~63s) ==="
ros2 bag play "$BAG" --clock -r 1.0 > "$OUT/play.log" 2>&1
sleep 3                            # final scan-match flush

echo "=== TF map->odom present? ==="
timeout 5 ros2 run tf2_ros tf2_echo map odom > "$OUT/tf_map_odom.log" 2>&1
grep -q "Translation" "$OUT/tf_map_odom.log" && echo "TF map->odom: YES" || echo "TF map->odom: NO"

echo "=== save map (wall clock, latched /map) ==="
timeout 20 ros2 run nav2_map_server map_saver_cli -f "$OUT/map" \
    --ros-args -p save_map_timeout:=15.0 > "$OUT/save.log" 2>&1 \
    && echo "map_saver: OK" || echo "map_saver: FAIL (see save.log)"

kill $LPID 2>/dev/null; sleep 2; kill -9 $LPID 2>/dev/null

echo "=== artifacts ==="
ls -la "$OUT"
