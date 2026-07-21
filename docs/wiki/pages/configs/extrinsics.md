---
type: config
tags: [config, calibration, extrinsics]
aliases: [extrinsic calibration, LiDAR-IMU extrinsics]
sources: [hesai_xt16.yaml]
updated: 2026-05-26
---

# Extrinsic Calibration — LiDAR–IMU

## Values

Transformation from IMU body frame to Hesai XT-16 optical center:

```
T = [0.171, 0.0, 0.0908] m     (x forward, z up)
R = I₃                          (no rotation)
```

Homogeneous matrix:

```
[1.0  0.0  0.0  0.171 ]
[0.0  1.0  0.0  0.000 ]
[0.0  0.0  1.0  0.0908]
[0.0  0.0  0.0  1.000 ]
```

## Source

Official Unitree Go2 documentation — Expansion Dock, standard mount.  
Confirmed physically: no rotation between IMU body frame and LiDAR frame.  
Online estimation disabled (`extrinsic_est_en: false`).

## Usage

Used in two places:

1. **FAST-LIO2 config** (`hesai_xt16.yaml` → `mapping.extrinsic_T/R`) — feeds state estimator
2. **Static TF** (launch file) — `base_link → hesai_lidar` for Nav2

Both must be consistent. Any discrepancy → map drift.

## ⚠️ Conflicting Source

An earlier notebook source contained different extrinsic values. **Ignore it** — official Unitree documentation is the ground truth. See memory note: "Hesai XT-16 extrinsic calibration."
