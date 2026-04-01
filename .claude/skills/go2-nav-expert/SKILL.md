---
name: go2-nav-expert
description: >
  Expert guidance for the Unitree Go2 autonomous navigation stack: FAST-LIO2 SLAM
  (Hesai XT16), Nav2 configuration for quadruped (SmacPlannerHybrid + MPPI), go2_nav_bridge
  cmd_vel→SportModeCmd translation, and MCP 3T architecture. Trigger when the user asks
  about navigation, SLAM, Nav2, FAST-LIO2, go2_nav_bridge, occupancy grid, path planning,
  or MCP integration on the Go2.
tools: Bash, Read, Edit, Glob, Grep
---

# Go2 Navigation Expert

Stack completo per navigazione autonoma sul Unitree Go2 con ROS 2 Humble.

**Milestone prerequisita:** Prima di aggiungere qualsiasi layer MCP, verificare end-to-end:
> LiDAR → FAST-LIO2 → Nav2 → go2_nav_bridge → **moto fisico del robot**

---

## Architettura del Sistema

```
Hesai XT16 (192.168.123.20:2368 UDP)
    └─ hesai_ros_driver_2  →  /lidar_points (PointCloud2)
                                        │
                                   FAST-LIO2
                                   (IEKF + ikd-Tree)
                                        │
                       TF: map → odom → base_link → hesai_lidar
                                        │
                                   Nav2 Stack
                            (SmacPlannerHybrid + MPPI)
                                        │
                              go2_nav_bridge
                          (cmd_vel → SportModeCmd)
                                        │
                              MCU 192.168.123.161
```

---

## FAST-LIO2 — Configurazione Hesai XT16

Config: `src/fast_lio_ros2/config/hesai_xt16.yaml`

### Parametri Chiave

```yaml
lidar_type: 2          # Hesai/Velodyne/Ouster
N_SCANS: 16            # canali Hesai XT16
filter_size_surf: 0.3
filter_size_map: 0.3

# Estrinseci ufficiali Unitree (T da base_link, R = identità)
extrinsic_T: [0.171, 0.0, 0.0908]
extrinsic_R: [1,0,0, 0,1,0, 0,0,1]

# IMU noise (BMI088 — stime da datasheet, da caratterizzare con Allan Variance)
gyr_cov:   0.0001
acc_cov:   0.001
b_gyr_cov: 0.00001
b_acc_cov: 0.0001
```

### Avvio

```bash
ros2 launch fast_lio mapping.launch.py config_file:=hesai_xt16.yaml rviz:=true
```

### Monitoraggio SLAM

```bash
ros2 topic echo /odometry/imu_incremental   # odometria IEKF
ros2 run tf2_ros tf2_echo map base_link      # pose corrente
```

**Concetti FAST-LIO2:**
- **ikd-Tree:** k-d tree incrementale — inserimento/cancellazione efficiente a >100 Hz
- **Direct odometry:** registra punti raw (no feature extraction) — robusto in ambienti poveri di features
- **Tightly-coupled fusion:** LiDAR + IMU via IEKF

---

## Nav2 — Configurazione per Quadrupede

### Planner e Controller (NON usare i default differential-drive)

```yaml
# planner_server
planner_plugins: ["GridBased"]
GridBased:
  plugin: "nav2_smac_planner/SmacPlannerHybrid"
  minimum_turning_radius: 0.35   # 0.3–0.5 m per Go2

# controller_server
controller_plugins: ["FollowPath"]
FollowPath:
  plugin: "nav2_mppi_controller::MPPIController"
```

### Lifecycle Manager — Nodi da includere

Con FAST-LIO2 attivo, **escludere** `map_server` e `amcl` (FAST-LIO2 pubblica già `map → odom → base_link`):

```yaml
node_names:
  - controller_server
  - planner_server
  - behavior_server
  - bt_navigator
# NON includere: map_server, amcl
```

### Occupancy Grid da FAST-LIO2

FAST-LIO2 non pubblica una costmap 2D. Per Nav2 occorre:

```bash
# Opzione 1: octomap_server (point cloud → OctoMap → /projected_map)
ros2 launch octomap_server octomap_mapping.launch

# Opzione 2: pointcloud_to_laserscan (3D → 2D slice)
```

---

## go2_nav_bridge — Traduzione cmd_vel → SportModeCmd

File: `src/go2_nav_bridge/`

### Mapping Twist → SportModeCmd

```python
cmd.mode = 2                      # VelocityMove
cmd.velocity[0] = linear.x        # avanti/indietro (m/s)
cmd.velocity[1] = linear.y        # laterale (m/s)
cmd.yaw_speed   = angular.z       # rotazione (rad/s)
```

### Limiti di sicurezza

```yaml
max_linear_vel:  1.0   # m/s (clamp su velocity[0] e velocity[1])
max_angular_vel: 1.0   # rad/s (clamp su yaw_speed)
```

### QoS (deve corrispondere a Nav2)

```python
QoSProfile(reliability=ReliabilityPolicy.RELIABLE, depth=10)
```

### Requisiti package

- Tipo: `ament_cmake` (non `ament_python`) — garantisce `--symlink-install` per YAML/launch
- Pubblicare TF statico `base_link → hesai_lidar` nel launch file del bridge

---

## Architettura MCP — 3 Livelli Temporali

```
┌──────────────────────────────────────────────────────┐
│  DELIBERATIVO  (~0.1 Hz)                             │
│  LLM (esterno) → MCP Client → MCP Server (Orin)     │
│  Pianificazione task, grounding semantico, goal gen  │
├──────────────────────────────────────────────────────┤
│  REATTIVO  (~20 Hz)                                  │
│  Nav2 (costmap, planner, MPPI controller)            │
│  Obstacle avoidance, path following                  │
├──────────────────────────────────────────────────────┤
│  ESECUTIVO  (~50 Hz)                                 │
│  go2_nav_bridge (cmd_vel → SportModeCmd)             │
│  Velocity clamping, watchdog, interfaccia MCU        │
└──────────────────────────────────────────────────────┘
```

**Regola fondamentale:** L'LLM emette Nav2 Action goals. Non ha autorità diretta su `cmd_vel`.

### mcp_watchdog (componente di sicurezza obbligatorio)

```
1. Monitora heartbeat dal client MCP (periodo suggerito: 5 s)
2. Cancella goal Nav2 attivo via Action Client se heartbeat perso per > N sec
3. Comanda set_stance("stand_still") o set_stance("sit") come safe-stop
```

---

## Decision Tree: Troubleshooting Connettività

```
Puoi pingare 192.168.123.18?
├─ NO  → Controlla cavo Ethernet, IP laptop statico (192.168.123.X), connessione RJ45 Dock
└─ SÌ  → Docker in esecuzione?
         ├─ NO  → ssh unitree@192.168.123.18 "docker ps" poi "docker start go2_navigation"
         └─ SÌ  → Topic ROS 2 visibili sul laptop?
                  ├─ NO  → Verifica ROS_DOMAIN_ID=1 su entrambi
                  │        Verifica cyclonedds.xml con NetworkInterface name="eth0"
                  └─ SÌ  → Stack operativo ✓
```

---

## Riferimenti Interni

- [fast_lio_tuning.md](references/fast_lio_tuning.md) — Parametri FAST-LIO2 dettagliati
- [networking_setup.md](references/networking_setup.md) — CycloneDDS e bridging Wi-Fi
- [deployment.md](references/deployment.md) — Ciclo sync-build-run
