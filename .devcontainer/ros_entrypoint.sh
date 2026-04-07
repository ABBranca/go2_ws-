#!/bin/bash
# shellcheck disable=SC1090,SC1091
set -e

# setup ros2 environment
source /opt/ros/"$ROS_DISTRO"/setup.bash

# Source local workspace only if already built
if [ -f ~/ros2_ws/install/setup.bash ]; then
  source ~/ros2_ws/install/setup.bash
fi

# add sourcing to .bashrc (idempotent — append only if not already present)
grep -qxF "source '/opt/ros/$ROS_DISTRO/setup.bash'" ~/.bashrc \
  || echo "source '/opt/ros/$ROS_DISTRO/setup.bash'" >> ~/.bashrc
grep -qxF "[ -f ~/ros2_ws/install/setup.bash ] && source ~/ros2_ws/install/setup.bash" ~/.bashrc \
  || echo "[ -f ~/ros2_ws/install/setup.bash ] && source ~/ros2_ws/install/setup.bash" >> ~/.bashrc

exec "$@"
