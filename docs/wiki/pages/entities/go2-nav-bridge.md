---
type: entity
tags: [software, ros2, control]
aliases: [go2_nav_bridge, nav bridge]
updated: 2026-05-26
---

# go2_nav_bridge — Navigation Control Bridge

## Role

Translates [[nav2]] `cmd_vel` (`geometry_msgs/Twist`) into Unitree Sport API `SportModeCmd` messages sent to the [[go2-robot]] MCU.

## Safety Features

- **Input sanitization** — rejects NaN/Inf velocity commands
- **Velocity watchdog** — stops robot if `cmd_vel` goes stale (timeout)
- **CommanderResult** — const-correct, safe accessors, explicit moves

## Interface

| Topic | Direction | Type |
|-------|-----------|------|
| `/cmd_vel` | subscribe | `geometry_msgs/Twist` |
| Sport API topic | publish | Unitree SDK type |

## Package

`src/go2_nav_bridge/`

Key files:
- `ISportCommander` — interface (lifecycle timeout constants, topic names)
- `SportCommander` — implementation (ctor/dtor, error_str)
