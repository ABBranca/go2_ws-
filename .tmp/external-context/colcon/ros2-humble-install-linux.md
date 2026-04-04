---
source: ROS 2 Official Docs (Humble) + Context7 API index
library: ROS 2
package: colcon
topic: install-colcon-for-ros2-humble-on-linux
fetched: 2026-04-04T00:00:00Z
official_docs: https://docs.ros.org/en/humble/Installation/Ubuntu-Install-Debs.html
---

## Relevant official commands (Ubuntu 22.04 / ROS 2 Humble)

### Minimal install commands (colcon available via ROS dev tools)
```bash
sudo apt update
sudo apt install -y ros-dev-tools
```

### If ROS 2 itself is not installed yet (official binary install path)
```bash
sudo apt update
sudo apt install -y ros-humble-ros-base
sudo apt install -y ros-dev-tools
```

### Shell setup
```bash
source /opt/ros/humble/setup.bash
```

For zsh:
```bash
source /opt/ros/humble/setup.zsh
```

### Quick verification
```bash
colcon --version
```

Optional ROS env check:
```bash
printenv | grep -E '^ROS_DISTRO|^ROS_VERSION'
```

## Source snippets used
- Humble Ubuntu deb install page: includes `sudo apt install ros-dev-tools` and environment sourcing.
- Humble Ubuntu source install page: includes `colcon build --symlink-install` and confirms colcon workflow.
