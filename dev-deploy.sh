#!/bin/bash
# dev-deploy.sh — Fast iterative deploy: rsync source + incremental colcon build
#
# Usage:
#   ./dev-deploy.sh                        # rebuild all packages
#   ./dev-deploy.sh go2_nav_bridge         # rebuild a single package (fast)
#
# Prerequisites on robot (one-time):
#   Il container dev viene avviato automaticamente dallo script se non è in esecuzione.
#   Richiede che l'immagine go2_nav_stack:dev (stage builder) sia presente sul robot.
#   Primo utilizzo: ./sync_to_dog.sh --build-dev-image

set -e

# Load .env if present
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/.env" ]; then
    set -a; source "$SCRIPT_DIR/.env"; set +a
fi

ROBOT_IP="${ROBOT_IP:-192.168.123.18}"
ROBOT_USER="${ROBOT_USER:-unitree}"
REMOTE_PATH="${ROBOT_WORKSPACE:-/home/unitree/go2_ws}"
PKG="${1:-}"

# Build colcon command
COLCON_ARGS="--symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble"
if [ -n "$PKG" ]; then
    COLCON_ARGS="--packages-select $PKG $COLCON_ARGS"
fi

# ── 1. Sync source only (fast — no build/ install/ log/) ─────────────────────
echo "[1/3] Sync sorgente → ${ROBOT_USER}@${ROBOT_IP}:${REMOTE_PATH}/src/"
rsync -az --progress \
    --exclude '.git' \
    --exclude 'build/' \
    --exclude 'install/' \
    --exclude 'log/' \
    src/ "${ROBOT_USER}@${ROBOT_IP}:${REMOTE_PATH}/src/"

# ── 2. Ensure dev container is running ────────────────────────────────────────
echo "[2/3] Verifica container go2_navigation_dev..."
ssh "${ROBOT_USER}@${ROBOT_IP}" bash -s << ENDSSH
    if docker ps --format '{{.Names}}' | grep -q '^go2_navigation_dev\$'; then
        echo "  container già in esecuzione."
    else
        echo "  avvio container dev..."
        docker run -itd \
            --name go2_navigation_dev \
            --network host \
            --ipc host \
            --cap-add NET_ADMIN \
            --cap-add SYS_NICE \
            -v ${REMOTE_PATH}:/ros2_ws \
            -v ${REMOTE_PATH}/docker/cyclonedds.xml:/cyclonedds.xml:ro \
            -e ROS_DOMAIN_ID=0 \
            -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \
            -e CYCLONEDDS_URI=file:///cyclonedds.xml \
            go2_nav_stack:dev bash
    fi
ENDSSH

# ── 3. Incremental colcon build inside the container ─────────────────────────
echo "[3/3] Build incrementale nel container (${PKG:-tutti i pacchetti})..."
ssh "${ROBOT_USER}@${ROBOT_IP}" \
    "docker exec go2_navigation_dev bash -c \
        'touch /ros2_ws/src/unitree_ros2/example/COLCON_IGNORE 2>/dev/null || true && \
         source /opt/ros/humble/setup.bash && \
         cd /ros2_ws && \
         colcon build ${COLCON_ARGS}'"

echo ""
echo "── Deploy completato ────────────────────────────────────────────────────────"
if [ -n "$PKG" ]; then
    echo "Pacchetto ricompilato: ${PKG}"
else
    echo "Tutti i pacchetti ricompilati."
fi
echo ""
echo "Per avviare il launch nel container dev:"
echo "  docker exec -it go2_navigation_dev bash"
echo "  source /ros2_ws/install/setup.bash && ros2 launch go2_nav_bridge bringup.launch.py"
