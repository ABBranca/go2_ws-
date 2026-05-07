import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    config_path = os.path.join(
        get_package_share_directory('hesai_ros_driver'),
        'config',
        'config.yaml'
    )
    return LaunchDescription([
        Node(
            namespace='hesai_ros_driver',
            package='hesai_ros_driver',
            executable='hesai_ros_driver_node',
            output='screen',
            parameters=[{'config_path': config_path}]
        ),
    ])