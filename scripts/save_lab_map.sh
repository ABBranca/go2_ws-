#!/usr/bin/env bash
# save_lab_map.sh — post-mapping artefact collection (2D slam_toolbox).
#
# Run AFTER building a map with bringup.launch.py / mapping_2d.launch.py while
# slam_toolbox is STILL ALIVE (it holds the map in memory). Saves:
#   1. The live /map occupancy grid as <name>.pgm + <name>.yaml
#      (nav2_map_server format — reloadable by nav2 map_server / AMCL).
#   2. If slam_toolbox's serialization service is up, a .posegraph + .data pair
#      for later continue-mapping or localization against this session.
#
# Usage:  ./scripts/save_lab_map.sh [output_name]

set -euo pipefail

WS_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NAME="${1:-lab_$(date +%Y%m%d_%H%M%S)}"
OUT_DIR="${WS_ROOT}/maps/${NAME}"
mkdir -p "$OUT_DIR"

if ! command -v ros2 >/dev/null; then
    echo "ros2 CLI not on PATH — source /opt/ros/humble/setup.bash and the workspace." >&2
    exit 2
fi

# 1. Occupancy grid → <name>.pgm + <name>.yaml (nav2_map_server format).
#    map_saver_cli subscribes to /map (TRANSIENT_LOCAL/latched) and writes on the
#    first sample. save_map_timeout guards against a stalled/missing publisher.
echo "Saving /map occupancy grid → ${OUT_DIR}/${NAME}.{pgm,yaml}"
ros2 run nav2_map_server map_saver_cli \
    -t /map \
    -f "${OUT_DIR}/${NAME}" \
    --ros-args -p save_map_timeout:=10.0

# 2. Optional: slam_toolbox pose-graph serialization (continue-mapping / localization).
#    Writes ${OUT_DIR}/${NAME}.posegraph and ${OUT_DIR}/${NAME}.data.
if ros2 service list 2>/dev/null | grep -q '/slam_toolbox/serialize_map'; then
    echo "slam_toolbox serialize_map service present — saving pose graph"
    ros2 service call /slam_toolbox/serialize_map \
        slam_toolbox/srv/SerializePoseGraph \
        "{filename: '${OUT_DIR}/${NAME}'}" >/dev/null
    echo "Pose graph saved: ${OUT_DIR}/${NAME}.posegraph"
else
    echo "slam_toolbox serialize_map service not found — skipping pose graph"
fi

echo "Done. Map artefacts in: ${OUT_DIR}"
