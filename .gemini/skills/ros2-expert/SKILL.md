# ROS 2 Humble Expert Skill

Official procedural guidance and best practices for developing with ROS 2 Humble Hawksbill.

## Core Architecture & Concepts

- **Node Modularity:** Design nodes with a single responsibility. Use `rclcpp::Node` (C++) or `rclpy.node.Node` (Python).
- **Communication Paradigms:**
    - **Topics:** Continuous data streams (LiDAR, Odometry). Use `Best Effort` QoS for high-frequency sensors.
    - **Services:** Quick, synchronous request/response (Set LED, Reset Encoder).
    - **Actions:** Long-running, asynchronous tasks with feedback and cancellation (Navigate to Goal, Arm Manipulation).
- **Discovery & Middleware:** ROS 2 uses DDS (Data Distribution Service). In this workspace, **CycloneDDS** is the default. Ensure `ROS_DOMAIN_ID` is consistent across the network.

## Essential CLI Commands

### Introspection
- `ros2 node list` / `info /<node>`: Check active nodes and their interfaces.
- `ros2 topic list -t` / `echo /<topic>`: List topics with types and monitor data.
- `ros2 topic hz /<topic>` / `bw /<topic>`: Measure frequency and bandwidth.
- `ros2 service list` / `type /<service>`: Inspect available services.
- `ros2 action list` / `info /<action>`: Inspect active action servers and clients.

### Configuration & Data
- `ros2 param list` / `get /<node> <param>`: Manage node parameters at runtime.
- `ros2 bag record -a` / `play <bag_file>`: Record and replay system data for debugging.
- `ros2 doctor --report`: Diagnose environment, network, and package issues.

## Development Best Practices

### Build & Package Management
- **Colcon:** Always use `colcon build --symlink-install` to avoid rebuilding for Python/config changes.
- **Dependencies:** Declare all system and ROS dependencies in `package.xml` and `CMakeLists.txt` (or `setup.py`).
- **Rosdep:** Run `rosdep install --from-paths src -y --ignore-src` before building.

### Quality of Service (QoS)
- **Reliable:** For commands and critical state (default for Services/Actions).
- **Best Effort:** For sensor data where latency is more critical than reliability.
- **Transient Local:** For late-joining subscribers (e.g., static maps, robot description).

### Advanced Patterns
- **Composition:** Use `ComposableNodes` to run multiple nodes in a single process container, reducing overhead via Zero-Copy communication.
- **Lifecycle Nodes:** Implement `LifecycleNode` for critical components requiring a deterministic startup sequence (Unconfigured -> Inactive -> Active).
- **Executors:** Use `MultiThreadedExecutor` and `CallbackGroups` (Reentrant or MutuallyExclusive) to prevent blocking operations from stalling the node.

## Naming Conventions
- **Packages/Nodes/Topics:** `snake_case` (e.g., `go2_nav_bridge`).
- **Messages/Services/Actions:** `PascalCase` (e.g., `SportModeCmd`).
- **Parameters:** `snake_case` (e.g., `max_velocity`).
