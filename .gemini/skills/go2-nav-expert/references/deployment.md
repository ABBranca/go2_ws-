# Unitree Go2 Deployment Workflow

## Overview
Development is local (laptop), execution is remote (Expansion Dock .18).

## Step 1: Syncing Code
Use the root-level script to push local changes to the robot:
```bash
./sync_to_dog.sh
```
This syncs `src/` to `/home/unitree/go2_ws/src` on the Dock.

## Step 2: Accessing Docker Container
SSH into the Dock and enter the running container:
```bash
ssh unitree@192.168.123.18
docker exec -it go2_navigation bash
```

## Step 3: Compiling (Inside Docker)
Always use symlink install for fast updates of Python/YAML:
```bash
colcon build --symlink-install --packages-select go2_nav_bridge fast_lio_ros2
source install/setup.bash
```

## Step 4: Execution
Launch the mapping stack:
```bash
ros2 launch fast_lio_ros2 mapping.launch.py
```
Launch the bridge:
```bash
ros2 run go2_nav_bridge bridge_node
```

## Step 5: Visualization (Laptop)
Ensure laptop is on the same `ROS_DOMAIN_ID`:
```bash
export ROS_DOMAIN_ID=1
rviz2 -d src/fast_lio_ros2/rviz/fastlio.rviz
```
