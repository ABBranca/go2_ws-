# FAST_LIO2 Tuning for Unitree Go2 & Hesai XT16

## LiDAR Configuration (Hesai XT16)
- **Channels**: 16
- **Scan Frequency**: 10Hz or 20Hz (prefer 10Hz for density)
- **FOV**: 360° Horizontal, 30° Vertical (-15° to +15°)

## IMU Noise Parameters (Unitree Go2 Onboard IMU)
These are estimated values for the Go2's internal IMU.
- **Acc Noise**: 0.1 m/s^2 / sqrt(Hz)
- **Gyr Noise**: 0.01 rad/s / sqrt(Hz)
- **Acc Bias Cov**: 0.0001
- **Gyr Bias Cov**: 0.00001

## Extrinsic Calibration (T_body_lidar)
Standard mount position for Go2 (to be verified with URDF):
- **Translation**: [0.2, 0.0, 0.15] (Approximate: x=forward, y=left, z=up)
- **Rotation**: Identity (if upright and forward-facing)

## FAST_LIO2 YAML Keys (src/fast_lio_ros2/config/velodyne.yaml as base)
Modify the following keys for XT16:
- `lidar_type`: 2 (Ouster/Hesai/Velodyne)
- `N_SCANS`: 16
- `filter_size_surf`: 0.3
- `filter_size_map`: 0.3
