#!/usr/bin/env python3
"""Inspect first /lidar_points message: fields, density, ring/time presence.
Determines what richer registration (3D LIO / multi-ring 2D / deskew) is possible."""
import sys
from rosbag2_py import SequentialReader, StorageOptions, ConverterOptions
from rclpy.serialization import deserialize_message
from sensor_msgs.msg import PointCloud2

bag = sys.argv[1]
r = SequentialReader()
r.open(StorageOptions(uri=bag, storage_id="sqlite3"), ConverterOptions("cdr", "cdr"))

while r.has_next():
    topic, data, t = r.read_next()
    if topic == "/lidar_points":
        m = deserialize_message(data, PointCloud2)
        print(f"frame_id      : {m.header.frame_id}")
        print(f"width x height: {m.width} x {m.height}  (is_dense={m.is_dense})")
        print(f"point_step    : {m.point_step} bytes   row_step: {m.row_step}")
        print(f"total points  : {m.width * m.height}")
        print("fields:")
        for f in m.fields:
            print(f"  - {f.name:12s} offset={f.offset:3d} datatype={f.datatype} count={f.count}")
        names = [f.name for f in m.fields]
        print("\nring/channel present :", any(n in names for n in ('ring','channel','c')))
        print("per-point time field :", any(n in names for n in ('time','t','timestamp','time_stamp')))
        print("intensity present    :", 'intensity' in names)
        break
