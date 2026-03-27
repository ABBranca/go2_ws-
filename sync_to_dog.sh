#!/bin/bash

# Configurazione (IP del Dock)
ROBOT_IP="192.168.123.18"
REMOTE_USER="unitree"
REMOTE_PATH="/home/unitree/go2_ws"

echo "--- Sincronizzazione codice verso il robot ($ROBOT_IP) ---"

# rsync è molto più veloce di scp perché manda solo le differenze
# --exclude evita di mandare le cartelle pesanti generate dal build locale
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
echo "Ora puoi compilare sul robot: 'docker exec -it go2_navigation /bin/bash -c \"source /opt/ros/humble/setup.bash && colcon build\"'"
