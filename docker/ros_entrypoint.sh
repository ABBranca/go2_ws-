#!/bin/bash
set -e

# Setup ROS 2 environment
# shellcheck source=/dev/null
source "/opt/ros/$ROS_DISTRO/setup.bash"

# Setup local workspace if built
if [ -f "/ros2_ws/install/setup.bash" ]; then
  # shellcheck source=/dev/null
  source "/ros2_ws/install/setup.bash"
fi

exec "$@"
