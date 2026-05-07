#!/bin/bash
set -e

# Ignoriamo la build dell'esempio unitree_ros2 come da tuo Dockerfile principale
touch src/unitree_ros2/example/COLCON_IGNORE || true

# Aggiorniamo rosdep
rosdep update || true

# Installiamo eventuali dipendenze mancanti dal manifest xml
rosdep install --from-paths src --ignore-src -r -y --skip-keys "unitree_go unitree_api" || true

# Eseguiamo la build
# IMPORTANTE: Aggiungiamo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON per permettere a clangd di trovare le definizioni
source /opt/ros/humble/setup.bash
colcon build --symlink-install --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble

echo "Configurazione dell'ambiente DevContainer completata."
