---
type: entity
tags: [software, navigation, ros2]
aliases: [Nav2, Navigation2]
updated: 2026-05-26
---

# Nav2 — Navigation 2 Framework

## Role

Nav2 handles path planning and trajectory execution on top of [[fast-lio2]] SLAM. Consumes odometry and point cloud, publishes `cmd_vel` to [[go2-nav-bridge]].

## Planners / Controllers

| Component | Plugin | Notes |
|-----------|--------|-------|
| Global planner | SmacPlannerHybrid | SE(2) hybrid A* |
| Local controller | MPPI | Model Predictive Path Integral |
| Costmap 2D | OctoMap → projected | 3D occupancy from `/cloud_registered` |

## Key Inputs

| Topic | Source |
|-------|--------|
| `/Odometry` | [[fast-lio2]] |
| `/cloud_registered` | [[fast-lio2]] |
| TF `map→odom→base_link` | [[fast-lio2]] + static TF |

## Key Outputs

| Topic | Consumer |
|-------|---------|
| `/cmd_vel` | [[go2-nav-bridge]] |

## Configuration

`src/go2_nav_bridge/config/nav2_params.yaml`

## Launch

`src/go2_nav_bridge/launch/bringup.launch.py` — full stack  
`src/go2_nav_bridge/launch/nav2.launch.py` — Nav2 only
