# mapping_2d_replay.launch.py — OFFLINE 2D SLAM replay from a rosbag (no hardware).
#
# Companion to mapping_2d.launch.py, stripped for bag-driven debugging away from the
# robot. Differences vs the live launch:
#   - NO Hesai driver (the bag supplies /lidar_points)
#   - NO bridge_node / SportModeCmd (no robot to drive)
#   - use_sim_time=true on every node so they consume bag timestamps via /clock
#
# Required bag topics:  /lidar_points  +  /utlidar/robot_odom  +  /utlidar/imu
#                       (/tf_static optional). /utlidar/imu drives the IMU-levelled
#                       base_stabilized frame — see the clock-domain caveat below.
#
# Usage:
#   terminal 1:  ros2 bag play <bag> --clock
#   terminal 2:  ros2 launch go2_nav_bridge mapping_2d_replay.launch.py
#
# TF chain (identical to live): map ─[slam_toolbox]→ odom ─[odom_tf_broadcaster]→
#   base_link ─[static]→ hesai_lidar
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_dir = get_package_share_directory('go2_nav_bridge')
    use_sim_time = LaunchConfiguration('use_sim_time')

    declare_sim_time = DeclareLaunchArgument(
        'use_sim_time', default_value='true',
        description='Use /clock from `ros2 bag play --clock` (offline replay).')

    # Sensor extrinsic: base_link → XT16 (official Unitree, no rotation).
    # static_transform_publisher honours /clock when use-sim-time is set.
    tf_base_link_hesai = ExecuteProcess(
        cmd=[
            'ros2', 'run', 'tf2_ros', 'static_transform_publisher',
            '--x', '0.171', '--y', '0.0', '--z', '0.0908',
            '--yaw', '0', '--pitch', '0', '--roll', '0',
            '--frame-id', 'base_link', '--child-frame-id', 'hesai_lidar',
            '--ros-args', '-p', 'use_sim_time:=true',
        ],
        output='screen',
    )

    # Go2 onboard leg odometry (from bag) → odom→base_link TF, plus the IMU-levelled
    # base_link→base_stabilized frame (tilt_source=imu, from /utlidar/imu in the bag).
    # CLOCK-DOMAIN CAVEAT: /utlidar/imu is MCU-stamped (same domain as robot_odom), NOT
    # the Hesai cloud's Orin clock. Replay a RE-STAMPED bag whose /utlidar/imu shares the
    # cloud's domain, else base_stabilized (IMU time) and /scan (cloud time) won't align
    # and pointcloud_to_laserscan will drop every scan. A/B test by flipping tilt_source
    # to 'odom' (or target_frame:=base_link in the yaml) on the same bag.
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
            'imu_mode': 'auto',
            'cf_tau': 0.25,
            'roll_offset': 0.0,          # TODO: set from level calibration [rad]
            'pitch_offset': 0.0,         # TODO: set from level calibration [rad]
            'use_sim_time': use_sim_time,
        }],
    )

    # 3D cloud (from bag) → 2D scan. NOTE: an optional `scan_preprocessor` node
    # (src/scan_preprocessor.cpp) can ring-filter the cloud first, but ring-decimation
    # MEASURED WORSE here (known coverage 55%→48%): the height band already gates
    # off-horizontal rings at range while the extra rings add short-range coverage. The
    # full 16-ring flatten is the best config on this near-static bag. Node kept available
    # for recordings with motion (where its future de-skew path matters).
    pc2scan = Node(
        package='pointcloud_to_laserscan',
        executable='pointcloud_to_laserscan_node',
        name='pointcloud_to_laserscan',
        output='screen',
        remappings=[
            ('cloud_in', '/lidar_points'),
            ('scan', '/scan'),
        ],
        parameters=[
            os.path.join(pkg_dir, 'config', 'pointcloud_to_laserscan.yaml'),
            {'use_sim_time': use_sim_time},
        ],
    )

    # 2D SLAM: /scan + odom→base_link → /map + TF map→odom.
    slam = Node(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        output='screen',
        parameters=[
            os.path.join(pkg_dir, 'config', 'slam_toolbox_2d.yaml'),
            {'use_sim_time': use_sim_time},
        ],
    )

    return LaunchDescription([
        declare_sim_time,
        tf_base_link_hesai,
        odom_tf,
        pc2scan,
        slam,
    ])
