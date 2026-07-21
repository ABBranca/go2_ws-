# Role: Senior Robotics Engineer (Specialist ROS 2 and NVIDIA Jetson)
# Task: SLAM Migration from FAST-LIO2 to GLIM (GPU-accelerated) for Unitree Go2

## Context
Currently, the Unitree Go2 robot uses FAST-LIO2 for SLAM. We want to migrate to **GLIM** (https://github.com/koide3/glim) to leverage GPU acceleration (CUDA) on the Jetson Orin Dock and achieve better Loop Closure performance.

### Current Technical Data
- **LiDAR:** Hesai XT16 (16 channels, mechanical). Topic: `/lidar_points`.
- **IMU:** Integrated in LiDAR (or via robot). Topic: `/lidar_imu`.
- **Extrinsics (IMU -> LiDAR):** T=[0.171, 0, 0.0908], R=Identity (already verified).
- **Current TF Chain:** map -> odom -> camera_init -> body -> base_link -> hesai_lidar.
- **Environment:** Ubuntu 22.04, ROS 2 Humble, Jetpack 6.1 (CUDA 12.2+).

## Objectives
Implement GLIM on a new branch, remove FAST-LIO2, and ensure Nav2 and Octomap continue to function correctly.

### 1. Branch Management and Cleanup
- Create a new branch `feature/glim-migration`.
- Remove the submodule/directory `src/fast_lio_ros2`.
- Clean up any fast_lio related build artifacts.

### 2. GLIM Installation and Configuration
- **Installation:** 
    - Add the official GLIM PPA: `curl -s https://koide3.github.io/ppa/setup_ppa.sh | sudo bash`.
    - Install binary packages: `ros-humble-glim-ros-cuda12.2` (or correct CUDA version for Jetpack 6) and `libgtsam-points-cuda12.2-dev`.
- **JSON Configuration:**
    - Create configs in `src/go2_nav_bridge/config/glim/`.
    - Configure `config_sensors.json`: set Hesai XT16 sensor (16 channels), correct topics, and extrinsics T=[0.171, 0, 0.0908].
    - Ensure `config.json` uses the **GPU pipeline** (odometry, sub_mapping, global_mapping).
- **TF Harmonization:**
    - GLIM must handle `map -> odom` and `odom -> base_link` transforms.
    - Simplify `src/go2_nav_bridge/launch/bringup.launch.py`: remove static identity bridges (`camera_init`, `body`) used for FAST-LIO and align with the ROS-standard `map -> odom -> base_link -> hesai_lidar`.

### 3. Docker and Launch Update
- Update `docker/Dockerfile`: add PPA setup and GLIM CUDA package installation.
- Update `docker/docker-compose.yml`: enable NVIDIA runtime for GPU access:
    ```yaml
    deploy:
      resources:
        reservations:
          devices:
            - driver: nvidia
              count: 1
              capabilities: [gpu]
    ```
- Update `src/go2_nav_bridge/launch/bringup.launch.py` and `mapping_only.launch.py` to include the GLIM node instead of FAST-LIO.

### 4. Verification and Test
- Provide commands to verify:
    - TF tree integrity (`ros2 run tf2_tools view_frames`).
    - GPU usage (via `tegrastats` or GLIM logs).
    - Map and odometry publication compatible with Nav2.

## Constraints
- Do not break `go2_nav_bridge` control logic (Watchdog, `cmd_vel` sanitization).
- Maintain use of `cyclonedds` as RMW middleware.
- Document every file created or modified in `SESSION_DIARY.md`.
