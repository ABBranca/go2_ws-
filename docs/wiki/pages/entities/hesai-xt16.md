---
type: entity
tags: [hardware, sensor, lidar]
aliases: [XT-16, Hesai XT16, XT16]
updated: 2026-05-26
---

# Hesai XT-16 — 16-Channel Mechanical LiDAR

## Specifications

| Parameter | Value |
|-----------|-------|
| Channels | 16 |
| Scan rate | 10 Hz (mechanical rotation) |
| Max range | 70 m @ 80% reflectivity |
| Horizontal FOV | 360° |
| Vertical FOV | −15° to +15° |
| IMU | None (no integrated IMU) |
| Timestamp | PTP absolute (µs) — driver converts to relative seconds |

## ROS 2 Driver

Package: `hesai_ros_driver_2`  
Config: `src/hesai_ros_driver_2/config/config.yaml`  
Published topic: `/lidar_points` (`sensor_msgs/PointCloud2`)

## Packet Format

Structurally equivalent to Velodyne format → FAST-LIO2 uses `lidar_type: 2` (Velodyne-compatible).

## Timestamp Handling

Driver publishes PTP absolute timestamps (µs integer). [[fast-lio2]] Velodyne handler:

1. Subtracts `t0` (first point timestamp) → relative time
2. Converts µs → seconds
3. Result: per-point relative timestamps in `[0, 0.1]` s window

This is critical for [[motion-undistortion]]. Config: `timestamp_unit: 0` (seconds after conversion).

## Mount

Mounted on Unitree Expansion Dock. Extrinsic calibration → [[extrinsics]].  
TF frame: `hesai_lidar`. See [[tf-tree]].

## PTP Configuration (Pandar Console)

Impostazioni verificate in Pandar Console (`192.168.123.20/setting.html`):

| Campo | Valore |
|-------|--------|
| Clock Source | PTP |
| Profile | 1588v2 |
| PTP Network Transport | UDP/IP |
| PTP Domain Number | 0 |
| Destination IP | 192.168.123.18 (Orin) |
| LiDAR Destination Port | 2368 |

Stato PTP visibile in Home page Pandar Console: **Free Run** (no sync) → **Tracking** (servo attivo) → **Lock**.

**Requisito:** [[orin-dock]] deve girare come PTP grandmaster. Vedere [[ptp-sync]].

## Known Issues

- PTP timestamp truncation bug fixed in commit `4bc8fcc` (field was `time` not `timestamp`, causing integer truncation to zero)
- PTP "Free Run" se `ptp4l` non attivo su Orin host → timestamps LiDAR non sincronizzati con ROS clock → IMU starvation in [[fast-lio2]]
- `ptp4l` **non** deve girare nel container Docker — conflitto BMCA con host master. Vedere [[ptp-sync]].

<!-- updated 2026-05-26: ptp4l rimosso da container, ora gira su host Orin -->
