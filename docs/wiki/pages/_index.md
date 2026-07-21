---
type: index
tags: [index]
updated: 2026-07-21
---

# Go2 Navigation Wiki — Index

Sistema di navigazione autonoma per [[go2-robot]] con [[hesai-xt16]], SLAM 2D ([[slam-toolbox-2d]], dal 2026-05-29; ex FAST-LIO2, ritirato) e [[nav2]].

---

## Entities

### Hardware
- [[go2-robot]] — Unitree Go2 quadruped robot platform
- [[hesai-xt16]] — Hesai XT-16 16-channel mechanical LiDAR
- [[imu]] — IMU source (`/utlidar/imu`, ~253 Hz)
- [[orin-dock]] — NVIDIA Orin compute module (Expansion Dock)

### Software
- [[slam-toolbox-2d]] — **current SLAM**: 2D slam_toolbox + Go2 leg odometry
- [[nav2]] — Nav2 navigation framework
- [[go2-nav-bridge]] — `cmd_vel` → SportModeCmd control bridge

> FAST-LIO2 (3D LiDAR-inertial SLAM) and its satellites (`livox_ros_driver2`, `octomap_server`)
> were retired on 2026-07-21 — a horizontal XT16 cannot observe Z (see [[z-observability]]).
> The packages remain recoverable from git history.

---

## Concepts

- [[tf-tree]] — TF frame chain: `map` → `odom` → `base_link` → `hesai_lidar`
- [[slam]] — SLAM overview and algorithm choice
- [[z-observability]] — why 3D LiDAR-inertial SLAM diverges in Z; the 2D pivot rationale
- [[motion-undistortion]] — LiDAR point cloud motion distortion correction
- [[ptp-sync]] — PTP grandmaster su Orin, sincronizzazione clock LiDAR (IEEE 1588v2)
- [[offline-slam-replay]] — workflow rosbag record/replay per sviluppo SLAM senza robot fisico
- [[clock-domain-mismatch]] — stamp cloud↔leg-odom in domini diversi (~56 anni) → 2D SLAM droppa tutti gli scan; fix re-stamp
- [[2d-map-tuning]] — risultati lab tuning mappa 2D: Huber/buffer/res0.03 ✅, levelling & ring-decimation ❌
- [[imu-tilt-compensation]] — slab livellato a gravità via [[imu]] (complementary filter): compensa il tilt del gait nel 2D map; risolve open-risk #2

---

## Configs

- [[extrinsics]] — LiDAR–IMU extrinsic calibration (T, R)
- [[dds-config]] — CycloneDDS network configuration

---

## System Overview

→ [[overview]]

---

## Ingest Log

→ [[_log]]
