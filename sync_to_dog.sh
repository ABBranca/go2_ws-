#!/bin/bash

# Load configuration from .env if present, otherwise use defaults
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ -f "$SCRIPT_DIR/.env" ]; then
    set -a; source "$SCRIPT_DIR/.env"; set +a
fi

ROBOT_IP="${ROBOT_IP:-192.168.123.18}"
REMOTE_USER="${ROBOT_USER:-unitree}"
REMOTE_PATH="${ROBOT_WORKSPACE:-/home/unitree/go2_ws}"

# Parsing argomenti
BUILD_IMAGE=false
BUILD_DEV_IMAGE=false
for arg in "$@"; do
  case $arg in
    --build-image)     BUILD_IMAGE=true ;;
    --build-dev-image) BUILD_DEV_IMAGE=true ;;
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
        -f docker/Dockerfile \
        -t go2_nav_stack:latest \
        --load \
        .

    echo "--- Trasferimento immagine al robot (via SSH pipe con gzip) ---"
    docker save go2_nav_stack:latest | gzip | ssh ${REMOTE_USER}@${ROBOT_IP} 'docker load'
    echo "--- Immagine trasferita! ---"
fi

# ── 2b. (Opzionale) Build stage builder e trasferimento come immagine dev ─────
if [ "$BUILD_DEV_IMAGE" = true ]; then
    echo ""
    echo "--- Build immagine dev ARM64 (stage: builder) ---"
    docker buildx build \
        --platform linux/arm64 \
        --target builder \
        -f docker/Dockerfile \
        -t go2_nav_stack:dev \
        --load \
        .

    echo "--- Trasferimento go2_nav_stack:dev al robot ---"
    docker save go2_nav_stack:dev | gzip | ssh ${REMOTE_USER}@${ROBOT_IP} 'docker load'
    echo "--- Immagine dev trasferita! ---"
fi

# ── 3. Istruzioni post-sync ───────────────────────────────────────────────────
echo ""
echo "Prossimi passi sul robot (ssh ${REMOTE_USER}@${ROBOT_IP}):"
echo ""
echo "  cd ${REMOTE_PATH}/docker && docker compose up --build -d"
echo ""
echo "  # Per il profilo dev (shell interattiva):"
echo "  cd ${REMOTE_PATH}/docker && docker compose --profile dev up -d"
echo "  docker exec -it go2_navigation_dev bash"
