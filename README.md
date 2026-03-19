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
*   **[fast_lio_ros2](https://github.com/hku-mars/FAST_LIO/tree/ROS2)**: A fast and robust LiDAR-Inertial Odometry framework. It provides high-frequency odometry and local mapping by fusing LiDAR and IMU data.
*   **[livox_ros_driver2](https://github.com/Livox-SDK/livox_ros_driver2)**: The driver for Livox LiDARs. In this project, it is primarily used to provide custom message types required by FAST_LIO.
*   **go2_nav_bridge**: (Local Package) A custom node that translates standard Nav2 movement commands (`cmd_vel`) into Unitree SDK commands.

---

## Preparation (One-time only)

### 1. Hardware Requirements
*   **Robot**: Unitree Go2.
*   **PC/Laptop**: Ubuntu 22.04 installed (recommended) and an Ethernet port.
*   **Ethernet Cable**: To connect the PC to the robot.

### 2. Installing Docker
If you don't have Docker, install it with these simple commands in your terminal:
```bash
sudo apt update
sudo apt install docker.io docker-compose -y
sudo usermod -aG docker $USER
```
*Restart your computer after these commands.*

---

## How to Start the System

### Step 1: Connecting to the Robot
1.  Connect the Ethernet cable between your laptop and the Go2 robot.
2.  Configure your laptop's network:
    *   Go to Ubuntu's network settings.
    *   Select the wired connection (Ethernet) and set **IPv4 to "Manual"**.
    *   **Address**: `192.168.123.10`
    *   **Netmask**: `255.255.255.0`
    *   **Gateway**: `192.168.123.1`
3.  Verify the connection by typing in the terminal: `ping 192.168.123.161` (you should see response times).

### Step 2: Download the Project and Submodules
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
