# bringup.launch.py — production full-autonomy stack (2D SLAM + Nav2).
#
# A horizontally-mounted Hesai XT16 (16 beams, ±15° vertical FOV) cannot observe
# vertical translation, so 3D LiDAR-inertial estimators (FAST-LIO2) run away in Z.
# Nav2 is 2D and the operating floor is planar, so we collapse the problem to a
# plane: slam_toolbox builds the map from a projected laser scan, and Z-free
# planar odometry is sourced from the Go2 legs.
#
#   [Hesai driver] ──/lidar_points──> [pointcloud_to_laserscan] ──/scan──┐
#                                                                        ├─> [slam_toolbox] ──/map, TF map→odom
#   [Go2 leg odom] ──/utlidar/robot_odom──> [odom_tf_broadcaster] ──TF odom→base_link
#   [Nav2] ──consumes /map + TF tree──> path planning / control ──/cmd_vel──┐
#   [Joystick → /cmd_vel] ───────────────────────────────────────────────> [bridge_node] ──> SportModeCmd
#
# REP-105 frame chain (fully 2D — no camera_init/body, no FAST-LIO, no octomap):
#   map ─[slam_toolbox]→ odom ─[odom_tf_broadcaster]→ base_link ─[static extrinsic]→ hesai_lidar
#
# slam_toolbox owns map→odom; do NOT also run map_odom_broadcaster here (two
# publishers on the same TF edge diverge the tree). Nav2's global costmap
# static_layer subscribes to slam_toolbox's /map (see nav2_params.yaml).

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess, IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node

# Startup-race gate. Nav2's Costmap2D blocks lifecycle activation until the full
# TF tree is present, and the global costmap static_layer needs slam_toolbox's
# /map. A coarse staged delay guarantees the producer chain (Hesai → pc2scan →
# slam_toolbox) is publishing map→odom + /map before Nav2 autostart activates.
NAV2_START_DELAY_S = 8.0


def _static_tf(parent: str, child: str, x='0', y='0', z='0',
               yaw='0', pitch='0', roll='0') -> ExecuteProcess:
    return ExecuteProcess(
        cmd=[
            'ros2', 'run', 'tf2_ros', 'static_transform_publisher',
            '--x', x, '--y', y, '--z', z,
            '--yaw', yaw, '--pitch', pitch, '--roll', roll,
            '--frame-id', parent, '--child-frame-id', child,
        ],
        output='screen',
    )


def generate_launch_description():
    hesai_dir = get_package_share_directory('hesai_ros_driver')
    pkg_dir = get_package_share_directory('go2_nav_bridge')

    # base_link -> hesai_lidar — official Unitree Go2 Expansion Dock extrinsic
    # (T=[0.171, 0, 0.0908], R=I3). base_link ≡ body convention.
    tf_base_link_hesai = _static_tf('base_link', 'hesai_lidar',
                                    x='0.171', y='0.0', z='0.0908')

    # Hesai XT16 → /lidar_points (frame hesai_lidar)
    hesai_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(hesai_dir, 'launch', 'start.py')
        ),
    )

    # Go2 onboard leg odometry → odom→base_link (continuous, Z anchored to floor).
    # Also publishes base_link→base_stabilized: an IMU-levelled frame (gait roll/pitch
    # removed, yaw kept) that pointcloud_to_laserscan projects into. tilt_source=imu
    # derives roll/pitch from /utlidar/imu (gravity-referenced) — NOT the biased leg-odom
    # attitude. roll_offset/pitch_offset [rad] = the static tilt captured while the robot
    # stands provably level; set them after calibration.
    odom_tf = Node(
        package='go2_nav_bridge',
        executable='odom_tf_broadcaster',
        name='odom_tf_broadcaster',
        output='screen',
        parameters=[{
            'input_topic': '/utlidar/robot_odom',
            'odom_frame': 'odom',
            'base_frame': 'base_link',
            'lock_z': False,
            'publish_stabilized': True,
            'stabilized_frame': 'base_stabilized',
            'tilt_source': 'imu',
            'imu_topic': '/utlidar/imu',
            'imu_mode': 'auto',          # quaternion if firmware fills it, else complementary
            'cf_tau': 0.25,              # complementary-filter time constant [s]
            'roll_offset': 0.0,          # TODO: set from level calibration [rad]
            'pitch_offset': 0.0,         # TODO: set from level calibration [rad]
        }],
    )

    # 3D cloud → 2D scan (horizontal slab in the IMU-levelled base_stabilized frame)
    pc2scan = Node(
        package='pointcloud_to_laserscan',
        executable='pointcloud_to_laserscan_node',
        name='pointcloud_to_laserscan',
        output='screen',
        remappings=[
            ('cloud_in', '/lidar_points'),
            ('scan', '/scan'),
        ],
        parameters=[os.path.join(pkg_dir, 'config', 'pointcloud_to_laserscan.yaml')],
    )

    # 2D SLAM: owns map→odom + /map
    slam = Node(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        output='screen',
        parameters=[os.path.join(pkg_dir, 'config', 'slam_toolbox_2d.yaml')],
    )

    bridge_node = Node(
        package='go2_nav_bridge',
        executable='bridge_node',
        name='go2_nav_bridge',
        output='screen',
        respawn=True,
        respawn_delay=1.0,
    )

    nav2_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_dir, 'launch', 'nav2.launch.py')
        ),
    )

    # Staged bringup to defuse the TF/costmap startup race: producers start
    # immediately; Nav2 is delayed until map→odom + /map are live.
    #   t=0   : static extrinsic, Hesai driver, leg odom, pc2scan, slam_toolbox, teleop bridge
    #   t=8 s : Nav2 (consumes TF tree + /map; autostart safe by now)
    nav2_delayed = TimerAction(
        period=NAV2_START_DELAY_S,
        actions=[nav2_launch],
    )

    return LaunchDescription([
        tf_base_link_hesai,
        hesai_launch,
        odom_tf,
        pc2scan,
        slam,
        bridge_node,
        nav2_delayed,
    ])
