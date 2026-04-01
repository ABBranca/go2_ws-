# Deployment Workflow — Go2

## Ciclo Standard

```
Edita localmente → sync → build nel container → testa → visualizza
```

## Step 1: Sync del Codice

```bash
./sync_to_dog.sh
# rsync -avz --exclude 'build/' --exclude 'install/' --exclude 'log/'
# src/ → unitree@192.168.123.18:~/go2_ws/src/
```

## Step 2: Accesso al Container

```bash
ssh unitree@192.168.123.18
docker exec -it go2_navigation bash
```

## Step 3: Build (dentro Docker)

```bash
# Build tutto
colcon build --symlink-install
source install/setup.bash

# Build package singolo (più veloce durante sviluppo)
colcon build --symlink-install --packages-select go2_nav_bridge
colcon build --symlink-install --packages-select fast_lio_ros2
```

## Step 4: Lancio dello Stack

```bash
# Driver LiDAR
ros2 launch hesai_ros_driver_2 start.py

# FAST-LIO2 SLAM
ros2 launch fast_lio mapping.launch.py config_file:=hesai_xt16.yaml rviz:=true

# Bridge Nav2 → MCU
ros2 run go2_nav_bridge bridge_node

# TF statico (se non nel launch file)
ros2 run tf2_ros static_transform_publisher \
  --x 0.171 --y 0.0 --z 0.0908 \
  --yaw 0 --pitch 0 --roll 0 \
  --frame-id base_link --child-frame-id hesai_lidar
```

## Step 5: Visualizzazione (Laptop)

```bash
source /opt/ros/humble/setup.bash
export ROS_DOMAIN_ID=1
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
# oppure: rviz2 -d src/fast_lio_ros2/rviz/fastlio.rviz
```

## Comando Tutto-in-Uno (sync + build)

```bash
./sync_to_dog.sh && ssh unitree@192.168.123.18 \
  "docker exec go2_navigation bash -c 'source /opt/ros/humble/setup.bash && colcon build --symlink-install && source install/setup.bash && echo BUILD_OK'"
```
