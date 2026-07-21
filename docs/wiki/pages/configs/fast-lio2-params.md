---
type: config
tags: [config, slam, fast-lio2]
aliases: [FAST-LIO2 config, hesai_xt16.yaml]
sources: [hesai_xt16.yaml]
updated: 2026-05-26
---

# FAST-LIO2 Parameters — hesai_xt16.yaml

Full reference: `src/fast_lio_ros2/config/hesai_xt16.yaml`

## Common

| Parameter | Value | Notes |
|-----------|-------|-------|
| `lid_topic` | `/lidar_points` | Published by `hesai_ros_driver_2` |
| `imu_topic` | `/utlidar/imu` | Go2 internal [[imu]] |
| `time_sync_en` | `false` | |
| `time_offset_lidar_to_imu` | `0.0` | |

## Preprocessing

| Parameter | Value | Notes |
|-----------|-------|-------|
| `lidar_type` | `2` | Velodyne-compatible (Hesai XT packet format) |
| `scan_line` | `16` | [[hesai-xt16]] channels |
| `scan_rate` | `10` | Mechanical rotation Hz |
| `timestamp_unit` | `0` | 0 = seconds (after PTP→relative conversion) |
| `blind` | `0.5` | Min valid range [m] — filters body self-returns |
| `point_filter_num` | `4` | Keep 1 in N points |

## Mapping

| Parameter | Value | Notes |
|-----------|-------|-------|
| `max_iteration` | `3` | iEKF convergence iterations |
| `filter_size_surf` | `0.5` | Surface voxel filter [m] |
| `filter_size_map` | `0.5` | Map voxel filter [m] |
| `cube_side_length` | `1000.0` | Local map cube [m] |
| `fov_degree` | `360.0` | Full horizontal FOV |
| `det_range` | `70.0` | Max detection range [m] |
| `extrinsic_est_en` | `false` | Online extrinsic estimation disabled |

## IMU Noise (Allan Variance, 2026-04-09)

| Parameter | Value |
|-----------|-------|
| `acc_cov` | `0.000825` |
| `gyr_cov` | `6.716e-07` |
| `b_acc_cov` | `1.376e-07` |
| `b_gyr_cov` | `7.812e-10` |

See [[imu]] for raw Allan Variance values. See [[extrinsics]] for `extrinsic_T/R`.

## Publish

| Parameter | Value |
|-----------|-------|
| `path_en` | `true` |
| `map_en` | `true` |
| `scan_publish_en` | `true` |
| `dense_publish_en` | `false` |
| `scan_bodyframe_pub_en` | `true` |

## PCD Save

| Parameter | Value |
|-----------|-------|
| `pcd_save_en` | `true` |
| `map_file_path` | `./hesai_xt16_map.pcd` |
| `interval` | `-1` (continuous) |
