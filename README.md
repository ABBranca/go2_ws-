# Unitree Go2 - Autonomous Navigation Setup

Welcome to the repository for the autonomous navigation of the Unitree Go2 robot. This guide is designed to be simple and clear, even if you don't have much experience with programming or Linux.

## Project Overview
The goal is to enable the Go2 robot to move autonomously using:
*   **ROS 2 Humble**: The "brain" that manages sensors and motors.
*   **LiDAR Hesai XT16**: The laser sensor that allows the robot to "see".
*   **FAST_LIO2**: High-performance LiDAR-inertial odometry for mapping and localization.
*   **Nav2**: The navigation stack for path planning and obstacle avoidance.
*   **Docker**: A tool that creates a "closed box" with all the software already installed, so you don't have to configure anything on your computer.

---

## Workspace Submodules
This project integrates several key technologies as Git submodules:

*   **[unitree_ros2](https://github.com/unitreerobotics/unitree_ros2)**: Official ROS 2 SDK for Unitree robots (Go2, B2, H1). It handles the communication with the robot's hardware and motion services.
*   **[hesai_ros_driver_2](https://github.com/HesaiTechnology/HesaiLidar_ROS_2.0)**: Official driver for Hesai LiDARs (XT16, XT32, etc.). It publishes `sensor_msgs/msg/PointCloud2` for FAST_LIO2.
*   **[fast_lio_ros2](https://github.com/hku-mars/FAST_LIO/tree/ROS2)**: A fast and robust LiDAR-Inertial Odometry framework. It provides high-frequency odometry and local mapping by fusing LiDAR and IMU data.
*   **[livox_ros_driver2](https://github.com/Livox-SDK/livox_ros_driver2)**: Used to provide custom message types required by FAST_LIO for solid-state LiDAR support (if needed) and general message compatibility.
*   **go2_nav_bridge**: (Local Package) A custom node that translates standard Nav2 movement commands (`cmd_vel`) into Unitree SDK commands.

---

## Preparation (One-time only)

### 1. Hardware Requirements
*   **Robot**: Unitree Go2.
*   **PC/Laptop**: Ubuntu 22.04 or Arch Linux (recommended) with an Ethernet port.
*   **Ethernet Cable**: For the initial setup.

### 2. Connecting the Robot to Wi-Fi
To connect the Go2 to your laboratory network (e.g., "ARSCONTROL"):
1.  **Mobile App**: Use the **Unitree Go** app on your smartphone (Android is recommended if iOS times out).
2.  **Permissions**: Ensure Bluetooth, GPS/Location, and Local Network permissions are granted.
3.  **Station Mode**: Select "Wi-Fi Mode" in the app, pick your SSID, and enter the password.
4.  **Pairing**: If the app fails to find the robot, ensure you are using an Android device (preferred). *Note: The physical shortcut of pressing the power button 3 times quickly has been tested and found ineffective for this unit.*

### 3. Installing Docker (on Laptop)
... (standard installation instructions) ...

---
## Development Workflow (Rapid Prototyping)
To speed up development and avoid rebuilding Docker images every time you change the code, follow this workflow:

### 1. Synchronize Code (Laptop -> Robot)
Modify your code locally on your laptop, then run the sync script to send only the changes to the robot via Ethernet:
```bash
./sync_to_dog.sh
```

### 2. Compile and Run (on Robot)
Access the running container on the robot to compile your changes:
```bash
docker exec -it go2_navigation bash
colcon build --symlink-install
source install/setup.bash
# Run your launch files here
```
*Note: The `--symlink-install` flag allows Python and YAML changes to take effect immediately without recompiling.*

### 3. Remote Visualization (on Laptop)
Open Rviz2 on your laptop to monitor the robot's state:
```bash
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
```

---

## Deployment Workflow (Piano B - Final Image)
When development is complete and you want to create a standalone image for the robot:

### 1. Build the ARM64 Image (on Laptop)
...
Ensure you are building for the robot's architecture:
```bash
docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load .
```

### 2. Transfer and Load (to Robot)
Pipe the image directly to the robot over the network:
```bash
docker save go2_nav_stack:latest | ssh -C unitree@<ROBOT_IP> 'docker load'
```

---

### Step 1: Connecting to the Robot
1.  Connect the Ethernet cable between your laptop and the **Expansion Dock** (RJ45 port on the back).
2.  Configure your laptop's network:
    *   **IPv4 Manual**: `192.168.123.10`
    *   **Netmask**: `255.255.255.0`
    *   **Gateway**: `192.168.123.1`
3.  **Critical IP Addresses**:
    *   `192.168.123.161`: Motion Control Unit (MCU). Has Wi-Fi but **SSH is blocked**.
    *   `192.168.123.18`: Expansion Dock (Orin). This is where you SSH to run code.

---

## Current Development Challenges (March 2026)
*   **Wireless Bridge**: The robot's MCU has the Wi-Fi adapter, but the Expansion Dock (where the navigation stack runs) is connected only via internal Ethernet. 
*   **SSH Block**: Direct SSH to the MCU (`.161`) is refused, preventing easy configuration of network routing/NAT.
*   **Goal**: Enable wireless Rviz2 telemetry by configuring ROS 2 DDS to route through the MCU's network interface.

---

## Download the Project and Submodules
Open the terminal and type:
```bash
git clone --recursive https://github.com/ABBranca/go2_ws.git
cd go2_ws
```
*If you already cloned it without submodules, run:*
```bash
git submodule update --init --recursive
```

### Step 3: Start the Environment (Docker)
Enter the `docker` folder and start everything:
```bash
cd docker
docker compose up --build -d
```
*This command will automatically download and configure everything needed. It may take a few minutes the first time.*

---

## Visualization (Rviz2)
To see what the robot "sees" from your laptop:
1.  Open a new terminal.
2.  Type:
    ```bash
    rviz2
    ```
3.  Load the configuration found in `src/go2_nav_bridge/rviz/nav2.rviz` (File -> Open Config).

---

**Contacts**: For technical questions or support, contact [ABBranca](https://github.com/ABBranca).
