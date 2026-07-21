# mapping_2d.launch.py — 2D SLAM lab map acquisition (FAST-LIO2 replacement).
#
# Rationale: a horizontally-mounted Hesai XT16 (16 beams, ±15° vertical FOV)
# cannot observe vertical translation → any 3D LiDAR-inertial estimator
# (FAST-LIO2) runs away in Z. Nav2 is 2D, the lab floor is flat, so we collapse
# the problem to a plane and source Z-free planar odometry from the Go2 legs.
#
#   [Hesai driver] ──/lidar_points──> [pointcloud_to_laserscan] ──/scan──┐
#                                                                        ├─> [slam_toolbox] ──/map, TF map→odom
#   [Go2 leg odom] ──/utlidar/robot_odom──> [odom_tf_broadcaster] ──TF odom→base_link
#   [Joystick → /cmd_vel] ──> [bridge_node] ──> SportModeCmd (manual teleop)
#
# TF chain (REP-105, fully 2D — no camera_init/body, no FAST-LIO, no octomap):
#   map ─[slam_toolbox]→ odom ─[odom_tf_broadcaster]→ base_link ─[static]→ hesai_lidar
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
    pkg_dir = get_package_share_directory('go2_nav_bridge')

    # Hesai XT16 → /lidar_points (frame hesai_lidar)
    hesai_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(hesai_dir, 'launch', 'start.py')
        ),
    )

    # Sensor extrinsic: IMU/base_link → XT16 (official Unitree, no rotation)
    tf_base_link_hesai = _static_tf('base_link', 'hesai_lidar',
                                    x='0.171', y='0.0', z='0.0908')

    # Go2 onboard leg odometry → odom→base_link (continuous, Z anchored to floor).
    # Also publishes base_link→base_stabilized: an IMU-levelled frame (gait roll/pitch
    # removed, yaw kept) that pointcloud_to_laserscan projects into. tilt_source=imu
    # derives roll/pitch from /utlidar/imu (gravity-referenced) — NOT the biased leg-odom
    # attitude. roll_offset/pitch_offset [rad] = the static tilt captured while the robot
    # stands provably level (see Step 5 of the plan); set them after calibration.
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

    # 2D SLAM: map→odom + /map
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

    return LaunchDescription([
        tf_base_link_hesai,
        hesai_launch,
        odom_tf,
        pc2scan,
        slam,
        bridge_node,
    ])
