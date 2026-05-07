#!/usr/bin/env bash
# setup_laptop_env.sh — DDS / ROS 2 environment for laptop ↔ Go2 robot.
#
# Usage (per-session):
#     source scripts/setup_laptop_env.sh
#
# Usage (persistent, recommended):
#     echo "source $HOME/Documents/Uni/Tesi/go2_ws/scripts/setup_laptop_env.sh" >> ~/.zshrc
#
# Mirrors docker/docker-compose.yml so peer-based DDS discovery succeeds on
# the 192.168.123.0/24 link with multicast disabled.
#
# IMPORTANT: this file must be SOURCED, not executed. Executing it spawns a
# subshell — exports do not propagate to the parent shell.

# ── Resolve own path (cross-shell) ──────────────────────────────────────────
# bash: ${BASH_SOURCE[0]} is the sourced file's path.
# zsh : ${(%):-%x} expands (via prompt syntax) to the same.
if [ -n "${ZSH_VERSION:-}" ]; then
    _SCRIPT_PATH="${(%):-%x}"
elif [ -n "${BASH_VERSION:-}" ]; then
    _SCRIPT_PATH="${BASH_SOURCE[0]}"
else
    _SCRIPT_PATH="$0"
fi
WS_ROOT="$(cd "$(dirname "${_SCRIPT_PATH}")/.." && pwd)"

# ── Pick the shell-matching ROS setup file ──────────────────────────────────
# /opt/ros/humble/setup.bash uses ${BASH_SOURCE[0]} which is empty in zsh,
# so AMENT_CURRENT_PREFIX collapses to the CWD — broken. Always source the
# variant matching the current shell.
if [ -n "${ZSH_VERSION:-}" ]; then
    _ROS_SHELL_EXT="zsh"
elif [ -n "${BASH_VERSION:-}" ]; then
    _ROS_SHELL_EXT="bash"
else
    _ROS_SHELL_EXT="sh"
fi

# 1. ROS 2 Humble base
if [ -f "/opt/ros/humble/setup.${_ROS_SHELL_EXT}" ]; then
    source "/opt/ros/humble/setup.${_ROS_SHELL_EXT}"
fi

# 2. Local overlay (after colcon build) — same shell-matching rule
if [ -f "${WS_ROOT}/install/setup.${_ROS_SHELL_EXT}" ]; then
    source "${WS_ROOT}/install/setup.${_ROS_SHELL_EXT}"
fi

# 3. DDS — must match docker-compose.yml on the robot
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export CYCLONEDDS_URI="file://${WS_ROOT}/cyclonedds_laptop.xml"
export ROS_LOCALHOST_ONLY=0

unset _SCRIPT_PATH _ROS_SHELL_EXT
