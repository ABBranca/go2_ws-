# Unitree Go2 ROS 2 Navigation Workspace

## Project Overview
This workspace is designed to enable autonomous navigation for the Unitree Go2 quadruped robot using **ROS 2 Humble**. It integrates the Unitree Go2 SDK, the `unitree_ros2` package, and `FAST_LIO2` for LiDAR-based SLAM with the Hesai XT16 sensor.

### Core Technologies
*   **Robot:** Unitree Go2
*   **LiDAR:** Hesai XT16 (16-channel)
*   **ROS 2 Version:** Humble
*   **SLAM:** [FAST_LIO2 (ROS 2)](https://github.com/hku-mars/FAST_LIO/tree/ROS2)
*   **Navigation:** Nav2
*   **Deployment:** Docker (Onboard computer)
*   **Visualization:** Rviz2 (Remote laptop via Ethernet)

## Architecture
The system consists of the following components:
1.  **Unitree ROS 2 Interface:** Communicates with the robot's proprietary SDK for motion control and state feedback.
2.  **FAST_LIO2:** Processes point clouds from the Hesai XT16 to provide high-frequency odometry and local mapping.
3.  **Nav2 Bridge (`go2_nav_bridge`):** A custom node that translates standard `geometry_msgs/Twist` commands from Nav2 into `unitree_go/msg/SportModeCmd` to control the robot's locomotion.
4.  **Nav2 Stack:** Handles path planning, obstacle avoidance, and goal management.

## Project Structure
```text
go2_ws/
├── docker/                 # Docker configuration for onboard deployment
│   ├── Dockerfile          # ROS 2 Humble base image with Nav2
│   └── docker-compose.yml  # Manages the navigation container
├── src/
│   ├── go2_nav_bridge/     # ROS 2 package for Nav2-to-SDK translation
│   │   ├── src/bridge_node.cpp
│   │   ├── CMakeLists.txt
│   │   └── package.xml
├───unitree_ros2/       # Git Submodule: Official Unitree SDK
├───hesai_ros_driver_2/ # Git Submodule: Official Hesai XT16 Driver
├───fast_lio_ros2/      # Git Submodule: FAST_LIO2 (ROS2 Branch)
│   └── livox_ros_driver2/  # Git Submodule: Dependency for FAST_LIO messages
└── GEMINI.md               # This documentation
```

## Milestone Status (March 19, 2026)
- **Submodules Integrated**: `unitree_ros2`, `fast_lio_ros2`, `livox_ros_driver2`, and `hesai_ros_driver_2` have been added.
- **Compilation Fixes**: `livox_ros_driver2/package.xml` was manually created to ensure ROS 2 compatibility and visibility for `colcon`.
- **Bridge Analysis**: Identified that `SportModeCmd` should be used instead of `LowCmd` for high-level navigation control.
- **Physical Testing**: The SLAM suite (FAST_LIO2 + Hesai driver) has not yet been tested on the physical robot.

## FAST-LIO2 Documentation
**FAST-LIO2** (Fast LiDAR-Inertial Odometry) è un framework di odometria e mapping LiDAR-inerziale veloce, robusto e versatile.

### Caratteristiche Principali
*   **ikd-Tree:** Utilizza una struttura dati "incremental k-d tree" dinamica che permette l'inserimento di nuovi punti e la rimozione di quelli vecchi in modo efficiente, supportando frequenze LiDAR elevate (>100Hz).
*   **Odometria Diretta:** Registra i punti grezzi direttamente sulla mappa (punti-su-mappa) senza la necessità di estrazione manuale di feature (come bordi o piani), migliorando la robustezza in ambienti poveri di feature.
*   **Fusione Stretta (Tightly-coupled):** Integra i dati LiDAR con l'IMU utilizzando un filtro di Kalman iterativo (IEKF).
*   **Supporto Sensori:** Compatibile con LiDAR rotanti (Hesai, Velodyne, Ouster) e a stato solido (Livox).

### Repository e Risorse
*   **Repository Ufficiale (ROS 1/2):** [hku-mars/FAST_LIO](https://github.com/hku-mars/FAST_LIO)
*   **Branch ROS 2:** [hku-mars/FAST_LIO/tree/ROS2](https://github.com/hku-mars/FAST_LIO/tree/ROS2)

## Building and Running

### Prerequisites
*   Ubuntu 22.04 on the robot's onboard computer and the remote laptop.
*   Docker and Docker Compose installed on the robot.
*   ROS 2 Humble installed on the remote laptop.

### Onboard Deployment (Docker)
1.  **Initialize Submodules** (if not already done):
    ```bash
    git submodule update --init --recursive
    ```
2.  **Build and run** the Docker image:
    ```bash
    cd docker
    docker compose up --build -d
    ```

### Remote Visualization
On your laptop (connected via Ethernet to the robot):
1.  Set up the ROS 2 environment:
    ```bash
    source /opt/ros/humble/setup.bash
    export ROS_DOMAIN_ID=1  # Match the ID in docker-compose.yml
    ```
2.  Launch Rviz2:
    ```bash
    rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
    ```

## Development Conventions
*   **Bridge Node:** All custom logic for navigation command translation should reside in `src/go2_nav_bridge`.
*   **Message Types**: Prefer high-level `SportModeCmd` for movement to leverage the robot's onboard stability controllers.
*   **TF Tree:**
    *   `map` -> `odom` (provided by FAST_LIO2)
    *   `odom` -> `base_link` (provided by FAST_LIO2)
    *   `base_link` -> `lidar_link` (static transform)

## TODO / Roadmap
- [x] Integrate ROS drivers for Hesai XT16 LiDAR.
- [ ] Implement `go2_nav_bridge` node for `cmd_vel` to `SportModeCmd` translation.
- [ ] Configure `FAST_LIO2` parameters for Hesai XT16 (noise, extrinsics).
- [ ] Integrate Nav2 parameters for quadrupedal movement.
- [ ] Set up Docker image with all dependencies (SDK, FAST_LIO, Nav2).
