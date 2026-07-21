---
type: entity
tags: [hardware, robot]
aliases: [Go2, Unitree Go2]
updated: 2026-05-26
---

# Go2 — Unitree Go2 Quadruped Robot

## Overview

Unitree Go2 is the target robot platform. Legged quadruped, locomotion managed by onboard MCU via Sport API. Navigation stack runs on external compute ([[orin-dock]]).

## Addresses

| Component     | IP                | Role                         |
| ------------- | ----------------- | ---------------------------- |
| [[orin-dock]] | `192.168.123.18`  | Compute (Docker stack)       |
| MCU           | `192.168.123.161` | Motion execution (Sport API) |

## Sensors (relevant to navigation)

- [[hesai-xt16]] — external LiDAR mounted on Expansion Dock
- [[imu]] — internal IMU at `/utlidar/imu` (~253 Hz)

## Motion Interface

`cmd_vel` (`geometry_msgs/Twist`) → [[go2-nav-bridge]] → `SportModeCmd` (Unitree SDK) → MCU

High-level Sport API: the bridge translates velocity commands without direct joint control.

## Extrinsics

LiDAR mount position relative to IMU body frame: see [[extrinsics]].  
TF frame: see [[tf-tree]].
