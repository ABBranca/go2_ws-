---
name: go2-deployment-pro
description: Expert guidance for ROS 2 deployment on Unitree Go2 Orin Dock using Docker, ARM64 optimization, and CycloneDDS.
---
# Unitree Go2 Deployment Pro Skill

Optimized DevOps practices for cross-developing on a laptop (x86) and deploying on the Go2 Orin Dock (ARM64).

## Docker & Build Lifecycle

### ARM64 Native Builds
- **Orin Optimization:** Use multi-stage builds in Docker to minimize the final image size on the robot.
- **Mounts:** For development, use `docker compose -f docker-compose.dev.yml` with host-mounted `src/` to avoid rebuilding the image for code changes.
- **Dependencies:** Always add `ros-humble-diagnostic-updater` and `libpcl-dev` to the Dockerfile to prevent common build failures in `fast_lio_ros2` and `hesai_ros_driver_2`.

### Syncing Strategies
- **rsync (sync_to_dog.sh):** Use `rsync -avz --exclude 'build/' --exclude 'install/' --exclude 'log/'` to sync source code only.
- **Remote Build:** Build directly on the Orin after syncing to ensure binary compatibility with ARM64 architecture.

## Networking (CycloneDDS)

### Interface Locking
- **Configuration:** Always provide a `cyclonedds.xml` file.
- **Mandatory XML Tag:** 
  ```xml
  <NetworkInterface name="eth0" />
  ```
- **Environment:** Set `CYCLONEDDS_URI` in the container's environment to point to the mounted XML file.

### Remote Visualization
- **ROS_DOMAIN_ID:** Ensure both laptop and robot use the same ID (default: 1).
- **Latency Mitigation:** If RViz2 is slow, decrease the frequency of `/lidar_points` (using a filter node) or use compressed point clouds.

## Deployment Checklist
1. **Source Check:** `source install/setup.bash`.
2. **Network Check:** `ros2 doctor --report` (Verify CycloneDDS picks the correct interface).
3. **Hardware Check:** Verify the LiDAR UDP port `2368` is reachable from the Dock.
4. **Safety Check:** Ensure the `mcp_watchdog` node is active before launching navigation goals.
