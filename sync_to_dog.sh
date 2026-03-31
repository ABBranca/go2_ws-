#!/bin/bash

# Configurazione (IP del Dock)
ROBOT_IP="192.168.123.18"
REMOTE_USER="unitree"
REMOTE_PATH="/home/unitree/go2_ws"

# Parsing argomenti
BUILD_IMAGE=false
for arg in "$@"; do
  case $arg in
    --build-image) BUILD_IMAGE=true ;;
  esac
done

# ── 1. Sync codice ────────────────────────────────────────────────────────────
echo "--- Sincronizzazione codice verso il robot ($ROBOT_IP) ---"
rsync -avz --progress \
    --exclude '.git' \
    --exclude 'build' \
    --exclude 'install' \
    --exclude 'log' \
    --exclude '.vscode' \
    --exclude 'package-lock.json' \
    --exclude 'node_modules' \
    ./ ${REMOTE_USER}@${ROBOT_IP}:${REMOTE_PATH}
echo "--- Sincronizzazione completata! ---"

# ── 2. (Opzionale) Build immagine ARM64 e trasferimento ───────────────────────
if [ "$BUILD_IMAGE" = true ]; then
    echo ""
    echo "--- Build immagine Docker ARM64 ---"
    docker buildx build \
        --platform linux/arm64 \
        -t go2_nav_stack:latest \
        --load \
        docker/

    echo "--- Trasferimento immagine al robot (via SSH pipe) ---"
    docker save go2_nav_stack:latest | ssh -C ${REMOTE_USER}@${ROBOT_IP} 'docker load'
    echo "--- Immagine trasferita! ---"
fi

# ── 3. Istruzioni post-sync ───────────────────────────────────────────────────
echo ""
echo "Prossimi passi sul robot (ssh ${REMOTE_USER}@${ROBOT_IP}):"
echo ""
if [ "$BUILD_IMAGE" = false ]; then
    echo "  # Se l'immagine non è ancora presente, buildala dal laptop con:"
    echo "  # ./sync_to_dog.sh --build-image"
    echo ""
fi
echo "  # Avvia il container"
echo "  docker run -d \\"
echo "    --name go2_navigation \\"
echo "    --network host \\"
echo "    --privileged \\"
echo "    -v ${REMOTE_PATH}:/ros2_ws \\"
echo "    -e ROS_DOMAIN_ID=1 \\"
echo "    -e RMW_IMPLEMENTATION=rmw_cyclonedds_cpp \\"
echo "    go2_nav_stack:latest"
echo ""
echo "  # Entra nel container e compila"
echo "  docker exec -it go2_navigation bash"
echo "  source /opt/ros/humble/setup.bash && colcon build --symlink-install"
