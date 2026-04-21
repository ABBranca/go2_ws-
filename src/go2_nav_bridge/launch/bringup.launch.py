import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import ExecuteProcess, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node


def generate_launch_description():
    hesai_dir = get_package_share_directory('hesai_ros_driver')
    fast_lio_dir = get_package_share_directory('fast_lio')
    go2_nav_bridge_dir = get_package_share_directory('go2_nav_bridge')

    # 1. Static TF: base_link -> hesai_lidar (official Unitree extrinsics)
    # Named-argument syntax required in Humble (positional is deprecated).
    static_tf = ExecuteProcess(
        cmd=[
            'ros2', 'run', 'tf2_ros', 'static_transform_publisher',
            '--x', '0.171', '--y', '0.0', '--z', '0.0908',
            '--yaw', '0', '--pitch', '0', '--roll', '0',
            '--frame-id', 'base_link', '--child-frame-id', 'hesai_lidar',
        ],
        output='screen',
    )

    # 2. Hesai XT16 LiDAR driver
    # Note: start.py also launches rviz2 without a disable flag.
    # On headless Orin (no $DISPLAY), the rviz2 process exits with an error
    # but does not affect the driver node.
    hesai_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(hesai_dir, 'launch', 'start.py')
        ),
    )

    # 3. FAST-LIO2 SLAM (rviz disabled for robot-side deployment)
    fast_lio_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(fast_lio_dir, 'launch', 'mapping.launch.py')
        ),
        launch_arguments={
            'config_file': 'hesai_xt16.yaml',
            'rviz': 'false',
        }.items(),
    )

    # 4. Nav2 stack
    nav2_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(go2_nav_bridge_dir, 'launch', 'nav2.launch.py')
        ),
    )

    # 4.5 Octomap Server (generates 2D occupancy grid from 3D cloud)
    octomap_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(go2_nav_bridge_dir, 'launch', 'octomap.launch.py')
        ),
    )

    # 5. go2_nav_bridge: cmd_vel -> SportModeCmd
    bridge_node = Node(
        package='go2_nav_bridge',
        executable='bridge_node',
        name='go2_nav_bridge',
        output='screen',
        respawn=True,
        respawn_delay=1.0,
    )

    return LaunchDescription([
        static_tf,
        hesai_launch,
        fast_lio_launch,
        nav2_launch,
        octomap_launch,
        bridge_node,
    ])
