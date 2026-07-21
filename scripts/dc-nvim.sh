#!/usr/bin/env bash
# dc-nvim.sh — Build/start the Go2 dev container and open LazyVim *inside* it.
#
# Why inside the container: clangd/pyright must see ROS 2 Humble headers, the
# sourced environment and the aggregated compile_commands.json — all of which
# live in the container, not on the host. Running nvim on the host cannot resolve
# ROS includes. See .devcontainer/ and docs.
#
# Requirements (host):
#   - Docker
#   - Dev Containers CLI:  npm install -g @devcontainers/cli
#   - LazyVim config in ~/.config/nvim (bind-mounted read/write into the container)
#
# Usage:
#   scripts/dc-nvim.sh [nvim args...]      # e.g. scripts/dc-nvim.sh src/go2_nav_bridge
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

if ! command -v devcontainer >/dev/null 2>&1; then
  echo "[dc-nvim] ERROR: 'devcontainer' CLI not found." >&2
  echo "[dc-nvim]        Install with: npm install -g @devcontainers/cli" >&2
  exit 1
fi

echo "[dc-nvim] Bringing up dev container (builds the image on first run)…"
devcontainer up --workspace-folder .

echo "[dc-nvim] Launching Neovim inside the container…"
if ! devcontainer exec --workspace-folder . nvim "$@"; then
  echo "[dc-nvim] 'devcontainer exec' failed (no TTY?). Falling back to 'docker exec -it'."
  CID="$(docker ps -qf "label=devcontainer.local_folder=$REPO_ROOT")"
  if [ -z "$CID" ]; then
    echo "[dc-nvim] ERROR: could not locate the running dev container." >&2
    exit 1
  fi
  docker exec -it "$CID" nvim "$@"
fi
