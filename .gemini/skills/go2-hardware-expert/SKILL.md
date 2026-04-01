---
name: go2-hardware-expert
description: Expert guidance for Unitree Go2 hardware calibration, LiDAR-IMU extrinsics, and onboard diagnostics.
---
# Unitree Go2 Hardware Expert Skill

Specialized procedures for maintaining, calibrating, and diagnosing the physical components of the Unitree Go2 platform, with a focus on SLAM accuracy.

## Sensor Calibration & Extrinsics

### LiDAR-IMU Alignment (FAST-LIO2)
- **Official Extrinsics (T):** `[0.171, 0.0, 0.0908]` meters (from `base_link` to `hesai_lidar`).
- **Official Extrinsics (R):** Identity matrix `I₃` (no relative rotation).
- **Verification:** Always verify TFs using `ros2 run tf2_ros tf2_echo base_link hesai_lidar`.
- **IMU Noise (BMI088):** 
    - `gyr_cov`: 0.0001, `acc_cov`: 0.001 (initial estimates).
    - `b_gyr_cov`: 0.00001, `b_acc_cov`: 0.0001.
    - *Procedure:* Record a 5-min static rosbag to refine these values via Allan Variance if SLAM drifts.

### Physical Inspection
- **Mounting:** Ensure the Hesai XT16 is level. A 1-degree tilt causes significant mapping errors at 10m range.
- **Cooling:** Monitor Orin temperature via `tegrastats`. Throttling starts at 85°C, affecting FAST-LIO2 processing time.

## Diagnostics & Health

### Battery & Power
- **Low Voltage:** `SportModeState` provides battery percentage. Thresholds: 20% (Warning), 10% (Critical - RTH).
- **MCU Communication:** Monitor `/utapi/sport_state`. Lack of updates for >0.5s indicates internal networking issues.

### Network Topology
- **MCU IP:** `192.168.123.161` (Blocked SSH).
- **LiDAR IP:** `192.168.123.20`.
- **Bridge Health:** Use `ros2 topic hz /cmd_vel` to ensure the bridge receives commands at >20Hz.
