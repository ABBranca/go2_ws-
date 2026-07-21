#!/usr/bin/env python3
"""Dump header.stamp (and recv time) for first/last msgs of each topic in a bag,
to expose clock-domain mismatches between /lidar_points and /utlidar/robot_odom."""
import sys
from rosbag2_py import SequentialReader, StorageOptions, ConverterOptions
from rclpy.serialization import deserialize_message
from sensor_msgs.msg import PointCloud2
from nav_msgs.msg import Odometry

bag = sys.argv[1]
r = SequentialReader()
r.open(StorageOptions(uri=bag, storage_id="sqlite3"),
       ConverterOptions("cdr", "cdr"))

want = {"/lidar_points": PointCloud2, "/utlidar/robot_odom": Odometry}
seen = {k: [] for k in want}   # (recv_s, hdr_s, frame_id)

while r.has_next():
    topic, data, t = r.read_next()   # t = recv time ns
    if topic in want and len(seen[topic]) < 1e9:
        m = deserialize_message(data, want[topic])
        hs = m.header.stamp.sec + m.header.stamp.nanosec * 1e-9
        seen[topic].append((t * 1e-9, hs, m.header.frame_id))

for topic, rows in seen.items():
    if not rows:
        print(f"{topic}: NO MESSAGES"); continue
    first, last = rows[0], rows[-1]
    print(f"\n=== {topic}  (n={len(rows)}, frame_id='{first[2]}') ===")
    print(f"  first: recv={first[0]:.3f}s  header.stamp={first[1]:.3f}s  "
          f"(skew recv-hdr = {first[0]-first[1]:+.3f}s)")
    print(f"  last : recv={last[0]:.3f}s  header.stamp={last[1]:.3f}s  "
          f"(skew recv-hdr = {last[0]-last[1]:+.3f}s)")

# Cross-topic domain gap (header stamps)
if seen["/lidar_points"] and seen["/utlidar/robot_odom"]:
    cloud_h = seen["/lidar_points"][0][1]
    odom_h  = seen["/utlidar/robot_odom"][0][1]
    print(f"\n>>> header-stamp domain gap (odom - cloud) = {odom_h - cloud_h:+.3f}s")
    print(">>> slam_toolbox needs these in the SAME domain; large gap => all scans dropped.")
