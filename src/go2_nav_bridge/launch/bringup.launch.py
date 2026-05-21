# bringup.launch.py
#
# REP-105 frame chain:
#   map ─[map_odom_broadcaster, dynamic identity 50 Hz]─> odom
#       ─[static identity]─> camera_init
#       ─[FAST-LIO2 laserMapping ~100 Hz]─> body
#       ─[static identity]─> base_link
#       ─[static extrinsic T=(0.171, 0, 0.0908)]─> hesai_lidar
#
# Static identity bridges (odom->camera_init, body->base_link) reconcile
# FAST-LIO2's hardcoded frame names with REP-105 without patching upstream
# C++ source. Replace map_odom_broadcaster's identity with a real loop-closure
# corrector when relocalization is added — no other launch changes required.

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

    # Dynamic map -> odom (identity now; upgradeable to loop closure).
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

    # Static identity bridges to reconcile FAST-LIO2 hardcoded frames with REP-105.
    tf_odom_camera_init = _static_tf('odom', 'camera_init')
    tf_body_base_link = _static_tf('body', 'base_link')

    # base_link -> hesai_lidar — official Unitree Go2 Expansion Dock extrinsic
    # (T=[0.171, 0, 0.0908], R=I3). Mirrors FAST-LIO2 IMU->LiDAR extrinsic_T
    # in hesai_xt16.yaml (body ≡ base_link convention).
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

    nav2_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(go2_nav_bridge_dir, 'launch', 'nav2.launch.py')
        ),
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
        nav2_launch,
        octomap_launch,
        bridge_node,
    ])
