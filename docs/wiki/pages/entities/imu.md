---
type: entity
tags: [hardware, sensor, imu]
aliases: [IMU, utlidar/imu]
updated: 2026-05-26
---

# IMU — Go2 Internal IMU

## Overview

The only IMU source in the system. Published by Go2 firmware.  
**Model is unknown** — confirmed NOT BMI088. [[hesai-xt16]] has no integrated IMU.

## ROS 2 Interface

| Parameter | Value |
|-----------|-------|
| Topic | `/utlidar/imu` |
| Rate | ~253 Hz (measured: `update_rate: 253.0`) |
| Frame | `imu_link` (or equivalent Go2 body frame) |

## Noise Parameters (Allan Variance, 2026-04-09)

| Parameter | Value | Unit |
|-----------|-------|------|
| Accelerometer noise density | 0.028719 | m/s²/√Hz |
| Accelerometer random walk | 3.714e-4 | m/s²·√Hz (bias) |
| Gyroscope noise density | 8.196e-4 | rad/s/√Hz |
| Gyroscope random walk | 2.796e-5 | rad/s·√Hz (bias) |

These raw values feed `imu_calib_tool`. FAST-LIO2 uses squared versions:

| FAST-LIO2 param | Value |
|-----------------|-------|
| `acc_cov` | 0.000825 |
| `gyr_cov` | 6.716e-7 |
| `b_acc_cov` | 1.376e-7 |
| `b_gyr_cov` | 7.812e-10 |

Full config: [[fast-lio2-params]]

## Consumers

- [[fast-lio2]] — IMU pre-integration, motion undistortion, state propagation (deprecated path)
- [[imu-tilt-compensation]] — gravity-referenced roll/pitch (complementary filter on `linear_acceleration` + `angular_velocity`, or firmware `orientation` if populated) → IMU-levelled `base_stabilized` scan slab for [[slam-toolbox-2d]]. This is the active consumer in the 2D stack.

## ⚠️ Critical Notes

- IMU topic was misconfigured in early versions — caused IMU starvation in FAST-LIO2 (fix: commit `82c756e`)
- Lab tests (IMU noise characterization + full SLAM validation) pending physical access
- **Orientation field unverified** — whether the firmware populates `sensor_msgs/Imu.orientation` (vs `orientation_covariance[0] == -1`) decides the [[imu-tilt-compensation]] path (`imu_mode: auto`). Verify on robot: `ros2 topic echo /utlidar/imu --once`.

<!-- updated 2026-06-03: added [[imu-tilt-compensation]] as active consumer; flagged orientation-field verification -->
