---
name: ros2-expert
description: >
  Expert guidance for ROS 2 Humble development, debugging, and optimization. Covers node
  architecture, QoS policies, CLI introspection, colcon build, lifecycle nodes, executors,
  and naming conventions. Trigger when the user asks about ROS 2 concepts, topics, services,
  actions, QoS, bag files, parameters, or debugging tools.
tools: Bash, Read, Edit
---

# ROS 2 Humble Expert

Best practice e procedure per sviluppo con ROS 2 Humble Hawksbill.

---

## Architettura & Paradigmi di Comunicazione

### Quando usare cosa

| Paradigma | Quando usarlo | Esempio in Go2 |
|-----------|---------------|----------------|
| **Topic** | Dati continui (stream) | `/lidar_points`, `/odometry/imu_incremental`, `/cmd_vel` |
| **Service** | Request/response sincrono, breve | Reset encoder, set LED |
| **Action** | Task asincrono lungo con feedback e cancellazione | `navigate_to_pose`, arm manipulation |

### QoS — Regole Pratiche

```python
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy, HistoryPolicy

# Sensori ad alta frequenza (latenza > affidabilità)
SENSOR_QOS = QoSProfile(
    reliability=ReliabilityPolicy.BEST_EFFORT,
    history=HistoryPolicy.KEEP_LAST,
    depth=1
)

# Comandi e stato critico (nessuna perdita tollerata)
CMD_QOS = QoSProfile(
    reliability=ReliabilityPolicy.RELIABLE,
    history=HistoryPolicy.KEEP_LAST,
    depth=10
)

# Topic che arrivano prima del subscriber (es. /robot_description, /map)
LATCHED_QOS = QoSProfile(
    reliability=ReliabilityPolicy.RELIABLE,
    durability=DurabilityPolicy.TRANSIENT_LOCAL,
    depth=1
)
```

**Per go2_nav_bridge:** `/cmd_vel` deve usare `RELIABLE, depth=10` per compatibilità Nav2.

---

## CLI Essenziale

### Ispezione

```bash
ros2 node list
ros2 node info /bridge_node           # interfacce: topic, service, action

ros2 topic list -t                    # con tipo messaggio
ros2 topic echo /cmd_vel
ros2 topic hz /lidar_points           # frequenza
ros2 topic bw /lidar_points           # banda

ros2 service list
ros2 service type /set_parameters

ros2 action list
ros2 action info /navigate_to_pose    # goal, result, feedback types

ros2 param list /bridge_node
ros2 param get /bridge_node max_linear_vel
ros2 param set /bridge_node max_linear_vel 0.5
```

### TF

```bash
ros2 run tf2_ros tf2_echo map base_link         # pose robot nel frame mappa
ros2 run tf2_ros tf2_echo base_link hesai_lidar  # verifica estrinseci
ros2 run rqt_tf_tree rqt_tf_tree                # albero TF visuale
```

### Diagnostica

```bash
ros2 doctor --report    # ambiente, rete, DDS, package
ros2 wtf                # alias di ros2 doctor
```

---

## Rosbag

```bash
# Registrare tutto
ros2 bag record -a -o session_$(date +%Y%m%d_%H%M%S)

# Registrare topic specifici (es. calibrazione IMU)
ros2 bag record /imu/data /lidar_points -o calibration_bag

# Replay
ros2 bag play <bag_dir> --rate 0.5    # rallentato

# Info sul bag
ros2 bag info <bag_dir>
```

---

## Build & Packaging

### Colcon

```bash
# Build standard — symlink per file Python/YAML/config
colcon build --symlink-install

# Package singolo
colcon build --symlink-install --packages-select go2_nav_bridge

# Con log dettagliato
colcon build --symlink-install --packages-select go2_nav_bridge \
  --cmake-args -DCMAKE_BUILD_TYPE=Debug

# Installare dipendenze prima del build
rosdep install --from-paths src -y --ignore-src
```

**Nota:** `--symlink-install` crea symlink solo per `ament_cmake`. Per `ament_python` YAML e launch file richiedono rebuild.

### CMakeLists.txt — Template per nodo C++

```cmake
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(unitree_go REQUIRED)

add_executable(bridge_node src/bridge_node.cpp)
ament_target_dependencies(bridge_node rclcpp geometry_msgs unitree_go)

install(TARGETS bridge_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})
```

---

## Pattern Avanzati

### Lifecycle Node (startup deterministico)

Implementare per componenti critici che richiedono sequenza init precisa:

```
Unconfigured → on_configure() → Inactive → on_activate() → Active
                                                          ↓ on_deactivate()
                                              Inactive → on_cleanup() → Unconfigured
```

```python
class BridgeNode(LifecycleNode):
    def on_configure(self, state):
        self.sub = self.create_subscription(...)   # crea risorse
        return TransitionCallbackReturn.SUCCESS
    
    def on_activate(self, state):
        self.pub = self.create_publisher(...)      # inizia pubblicazione
        return TransitionCallbackReturn.SUCCESS
```

### MultiThreadedExecutor + CallbackGroups

Evitare che operazioni bloccanti stalli il nodo:

```python
from rclpy.callback_groups import ReentrantCallbackGroup, MutuallyExclusiveCallbackGroup
from rclpy.executors import MultiThreadedExecutor

cb_group = ReentrantCallbackGroup()
self.sub = self.create_subscription(msg_type, topic, callback, qos, callback_group=cb_group)

executor = MultiThreadedExecutor(num_threads=4)
executor.add_node(node)
executor.spin()
```

### Composable Nodes (zero-copy tra nodi co-located)

```python
# Nel launch file
ComposableNodeContainer(
    name='go2_container',
    package='rclcpp_components',
    executable='component_container',
    composable_node_descriptions=[
        ComposableNode(package='go2_nav_bridge', plugin='go2_nav_bridge::BridgeNode'),
    ],
)
```

---

## Convenzioni di Naming

| Elemento | Stile | Esempio |
|----------|-------|---------|
| Package, nodi, topic, parametri | `snake_case` | `go2_nav_bridge`, `max_linear_vel` |
| Messaggi, servizi, action | `PascalCase` | `SportModeCmd`, `NavigateToPose` |
| Frame TF | `snake_case` | `base_link`, `hesai_lidar` |

---

## Compatibilità Humble — Note Specifiche

```bash
# static_transform_publisher: usare sintassi named-argument (positional deprecata)
ros2 run tf2_ros static_transform_publisher \
  --x 0.171 --y 0.0 --z 0.0908 \
  --frame-id base_link --child-frame-id hesai_lidar

# ament_export_interfaces → deprecato (Humble): warning innocuo nei submoduli Unitree
# rosidl_target_interfaces → deprecato: warning innocuo, funziona ancora
```
