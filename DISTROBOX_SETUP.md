# Distrobox Environment Setup

This document records the creation of the Distrobox container used for this project to ensure environment consistency and assist in troubleshooting (e.g., LSP/clangd configuration).

## Container Details

- **Name:** `go2_nav_ws`
- **Image:** `docker.io/osrf/ros:humble-desktop` (Ubuntu 22.04 Jammy)
- **NVIDIA Integration:** Enabled (`--nvidia` flag used)
- **Custom Home Directory:** `~/distrobox/go2_nav_ws` (isolates container configs from host)
- **Host Workspace Path:** Wherever you cloned `go2_ws` (accessible inside the container via Distrobox's automatic home mount)

## Setup Commands Executed

```bash
# Creation
distrobox create --name go2_nav_ws \
    --image docker.io/osrf/ros:humble-desktop \
    --nvidia \
    --home ~/distrobox/go2_nav_ws \
    --yes

# Internal Setup
distrobox enter go2_nav_ws -- sudo apt update
distrobox enter go2_nav_ws -- sudo apt install -y python3-colcon-common-extensions python3-rosdep python3-argcomplete
distrobox enter go2_nav_ws -- bash -c "sudo rosdep init && rosdep update"
distrobox enter go2_nav_ws -- bash -c "echo 'source /opt/ros/humble/setup.bash' >> ~/.bashrc"
```

## Clangd/LSP Note for Claude

When fixing `clangd` issues for C++ nodes (like `go2_nav_bridge`):
1. The compiler and ROS 2 headers are located **inside** the Distrobox container at `/opt/ros/humble/...`.
2. To generate a valid `compile_commands.json`, `colcon build --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` should be run inside the container.
3. Clangd on the host might need to be pointed to the container's environment or use a wrapper to resolve include paths correctly.
