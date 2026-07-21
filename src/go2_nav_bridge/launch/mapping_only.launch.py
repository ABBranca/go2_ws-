# mapping_only.launch.py

"""mapping_only.launch.py — lab map acquisition pipeline.

Drops Nav2 (no autonomous goal navigation) and keeps the SLAM/teleop chain:

    [Hesai driver] ──/lidar_points──┐
                                    ├──> [FAST-LIO2] ──/cloud_registered──> [octomap]
    [Hesai IMU] ──/utlidar/imu──────┘       └──/tf (odom→base_link ~100 Hz)
    [map_odom_broadcaster] ──/tf (map→odom 50 Hz)
    [Joystick → /cmd_vel] ──> [bridge_node] ──> SportModeCmd (manual teleop)

REP-105 chain: map → odom → base_link → hesai_lidar

NOTE: FAST-LIO2 hardcodes world_frame="camera_init" and body_frame="body"
in laserMapping.cpp (no YAML override exists). Two static identity bridges
(odom→camera_init, body→base_link) reconcile those names with REP-105.
Mirrors bringup.launch.py. Do NOT remove them.
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


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
    fast_lio_dir = get_package_share_directory('fast_lio')
    go2_nav_bridge_dir = get_package_share_directory('go2_nav_bridge')

    map_odom = Node(
        package='go2_nav_bridge',
        executable='map_odom_broadcaster',
        name='map_odom_broadcaster',
        output='screen',
        parameters=[{
            'publish_rate_hz': 50.0,
            'map_frame': 'map',
            'odom_frame': 'odom',
        }],
    )

    # FAST-LIO2 hardcodes world_frame="camera_init", body_frame="body".
    # These static transforms bridge to the REP-105 chain (odom, base_link).
    tf_odom_camera_init = _static_tf('odom', 'camera_init')
    tf_body_base_link = _static_tf('body', 'base_link')
    tf_base_link_hesai = _static_tf('base_link', 'hesai_lidar',
                                    x='0.171', y='0.0', z='0.0908')

    hesai_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(hesai_dir, 'launch', 'start.py')
        ),
    )

    fast_lio_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(fast_lio_dir, 'launch', 'mapping.launch.py')
        ),
        launch_arguments={
            'config_file': 'hesai_xt16.yaml',
            'rviz': 'false',
        }.items(),
    )

    octomap_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(go2_nav_bridge_dir, 'launch', 'octomap.launch.py')
        ),
    )

    bridge_node = Node(
        package='go2_nav_bridge',
        executable='bridge_node',
        name='go2_nav_bridge',
        output='screen',
        respawn=True,
        respawn_delay=1.0,
    )

    return LaunchDescription([
        map_odom,
        tf_odom_camera_init,
        tf_body_base_link,
        tf_base_link_hesai,
        hesai_launch,
        fast_lio_launch,
        octomap_launch,
        bridge_node,
    ])
