import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='octomap_server',
            executable='octomap_server_node',
            name='octomap_server',
            output='screen',
            parameters=[{
                'resolution': 0.05,
                'frame_id': 'map',
                'base_frame_id': 'base_link',
                'sensor_model/max_range': 5.0,
                'save_free_cells': True,
                'latch': False,
            }],
            remappings=[
                ('cloud_in', '/cloud_registered'),
            ]
        ),
    ])
