# mapping_only.launch.py

"""mapping_only.launch.py — lab map acquisition pipeline.

Drops Nav2 (no autonomous goal navigation) and keeps the SLAM/teleop chain:

    [Hesai driver] ──/lidar_points──┐
                                    ├──> [GLIM] ──/glim_ros/cloud──> [octomap]
    [Hesai IMU] ──/lidar_imu────────┘        └──/tf (map→odom, odom→base_link)
    [Joystick → /cmd_vel] ──> [bridge_node] ──> SportModeCmd (manual teleop)

Map saved on GLIM exit to /ros2_ws/glim_map (save_on_exit=true in config.json).
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
    go2_nav_bridge_dir = get_package_share_directory('go2_nav_bridge')

    # GLIM publishes odom→base_link and map→odom natively. Only sensor extrinsic needed.
    tf_base_link_hesai = _static_tf('base_link', 'hesai_lidar',
                                    x='0.171', y='0.0', z='0.0908')

    hesai_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('hesai_ros_driver'), 'launch', 'start.py')
        ),
    )

    glim_node = Node(
        package='glim_ros',
        executable='glim_rosnode',
        name='glim_ros',
        parameters=[{
            'config_path': os.path.join(go2_nav_bridge_dir, 'config', 'glim'),
        }],
        output='screen',
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
        tf_base_link_hesai,
        hesai_launch,
        glim_node,
        octomap_launch,
        bridge_node,
    ])
