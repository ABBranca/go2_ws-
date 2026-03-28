# Unitree Go2 - Autonomous Navigation Setup

Welcome to the repository for the autonomous navigation of the Unitree Go2 robot. This guide explains how to set up the environment, develop code, and deploy it to the robot.

---

## Workspace Structure & Submodules
This project integrates several key technologies as Git submodules:

*   **[unitree_ros2](https://github.com/unitreerobotics/unitree_ros2)**: Official ROS 2 SDK for Unitree robots (Go2, B2, H1). It handles the communication with the robot's hardware and motion services.
*   **[hesai_ros_driver_2](https://github.com/HesaiTechnology/HesaiLidar_ROS_2.0)**: Official driver for Hesai LiDARs (XT16, XT32, etc.). It publishes `sensor_msgs/msg/PointCloud2` for FAST_LIO2.
*   **[fast_lio_ros2](https://github.com/hku-mars/FAST_LIO/tree/ROS2)**: A fast and robust LiDAR-Inertial Odometry framework. It provides high-frequency odometry and local mapping by fusing LiDAR and IMU data.
*   **[livox_ros_driver2](https://github.com/Livox-SDK/livox_ros_driver2)**: Used to provide custom message types required by FAST_LIO for solid-state LiDAR support (if needed) and general message compatibility.
*   **go2_nav_bridge**: (Local Package) Translates Nav2 `cmd_vel` (`geometry_msgs/Twist`) into Unitree `SportModeCmd` (mode=2, VelocityMove). Implements clamping (±1.0 m/s linear, ±1.0 rad/s angular), RELIABLE QoS matching Nav2, and a 200 ms safety watchdog that publishes zero velocity on `cmd_vel` timeout. Built as `ament_cmake`.

---

## 1. Environment Preparation (One-time only)

### Hardware Requirements
*   **Robot**: Unitree Go2.
*   **LiDAR**: Hesai XT16 (connected to the Expansion Dock).
*   **PC/Laptop**: Ubuntu 22.04 (recommended) with an Ethernet port.
*   **Ethernet Cable**: To connect the PC to the Robot.

### Laptop Network Configuration
Connect the Ethernet cable to the robot's **Expansion Dock** and configure your laptop's wired connection:
*   **IPv4 Manual**: `192.168.123.10`
*   **Netmask**: `255.255.255.0`
*   **Gateway**: `192.168.123.1` (Optional)

### Critical Robot IP Addresses
*   `192.168.123.161`: Motion Control Unit (MCU). Handles motors. **SSH blocked**.
*   `192.168.123.18`: Expansion Dock (Orin). Where the navigation stack lives. **SSH enabled**.

---

## 2. Project Download

Clone the repository with all necessary submodules:
```bash
git clone --recursive https://github.com/ABBranca/go2_ws.git
cd go2_ws
```
*If you already cloned it without submodules, run:*
```bash
git submodule update --init --recursive
```

---

## 3. Rapid Development Workflow (Prototyping)

To speed up development and avoid rebuilding Docker images for every code change:

### A. Code Synchronization (Laptop -> Robot)
Modify code locally on your laptop (in `src/`), then send changes to the robot via Ethernet:
```bash
chmod +x sync_to_dog.sh
./sync_to_dog.sh
```

### B. Build and Run (on Robot)
Access the Docker container on the robot to build and run:
```bash
# Enter the container
docker exec -it go2_navigation bash

# Build (Python/YAML changes are instant thanks to --symlink-install)
colcon build --symlink-install
source install/setup.bash

# Example: launch the navigation bridge
ros2 launch go2_nav_bridge mapping.launch.py
```

### C. Local Development Toolchain (on Laptop)
To enable clangd-based IntelliSense without a full ROS 2 installation, use a Distrobox container with ROS 2 Humble:
```bash
# Generate compile_commands.json inside the Distrobox container
distrobox enter humble-dev
colcon build --symlink-install --cmake-args -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cp build/compile_commands.json ../compile_commands.json  # workspace root
```
`compile_commands.json` is listed in `.gitignore` (generated artifact). The `.vscode/settings.json` file configures the clangd path to the binary inside the Distrobox container.

### D. Remote Visualization (on Laptop)
Open Rviz2 on your laptop to monitor the robot:
```bash
source /opt/ros/humble/setup.bash
export ROS_DOMAIN_ID=1
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
```

---

## 4. Final Deployment (Immutable Image)

When development is complete and you want to create a final image ("Piano B"):

### A. Build ARM64 Image (on Laptop)
```bash
docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load .
```

### B. Transfer and Load (to Robot)
Pipe the compressed image directly to the robot's Docker:
```bash
docker save go2_nav_stack:latest | ssh -C unitree@192.168.123.18 'docker load'
```

---
**Contacts**: For technical questions or support, contact [ABBranca](https://github.com/ABBranca).
