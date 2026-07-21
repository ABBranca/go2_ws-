#!/bin/bash
set -e

# I named volume di build/install/log (vedi devcontainer.json) possono essere inizializzati
# come root su alcune versioni di Docker: garantiamo la proprietà a ros (ha sudo NOPASSWD),
# altrimenti colcon build fallirebbe con permission denied. Idempotente.
sudo chown ros:ros build install log 2>/dev/null || true

# Ignoriamo la build dell'esempio unitree_ros2 come da tuo Dockerfile principale
touch src/unitree_ros2/example/COLCON_IGNORE || true

# Aggiorniamo rosdep
rosdep update || true

# L'immagine fa `rm -rf /var/lib/apt/lists/*`: senza questo update l'indice apt è vuoto a
# runtime e rosdep non riesce a risolvere alcun pacchetto ros-humble-* del manifest.
sudo apt-get update || true

# Installiamo eventuali dipendenze mancanti dal manifest xml
rosdep install --from-paths src --ignore-src -r -y --skip-keys "unitree_go unitree_api" || true

# Eseguiamo la build
# IMPORTANTE: Aggiungiamo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON per permettere a clangd di trovare le definizioni
source /opt/ros/humble/setup.bash
colcon build --symlink-install --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble

# --- clangd: aggreghiamo i compile_commands.json per-pacchetto in un unico DB.
# colcon ne genera uno per build/<pkg>/, ma clangd ne legge uno solo: senza questo
# merge l'LSP non risolve gli include ROS 2 cross-package.
# Destinazione = build/compile_commands.json, coerente con il .clangd del repo
# (CompilationDatabase: build/) e con il symlink di root compile_commands.json.
# -mindepth 2 include i file per-pacchetto ed esclude l'aggregato build/compile_commands.json
# (evita auto-inclusione ricorsiva alle riesecuzioni della postCreate).
if compgen -G "build/*/compile_commands.json" > /dev/null; then
  find build -mindepth 2 -name compile_commands.json -exec cat {} + \
    | jq -s 'add' > build/compile_commands.json
  echo "compile_commands.json aggregato in build/: $(jq 'length' build/compile_commands.json) entry."
else
  echo "ATTENZIONE: nessun compile_commands.json trovato sotto build/ (build C++ fallita?)."
fi

echo "Configurazione dell'ambiente DevContainer completata."
