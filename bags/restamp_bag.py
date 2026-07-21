#!/usr/bin/env python3
"""Re-stamp a bag onto a single clock domain so slam_toolbox can fuse scan+odom.

Problem: /lidar_points is stamped on the Orin system clock (NTP-unsynced, ~8568s)
while /utlidar/robot_odom is stamped on the MCU's real UTC clock (~1.78e9s). The
56-year header-stamp gap makes slam_toolbox drop every scan.

Fix: for both topics, overwrite header.stamp with the bag *receive* time (which is
in ONE consistent domain — the record clock). All other topics pass through verbatim.

Usage: restamp_bag.py <in_bag_dir> <out_bag_dir>
"""
import sys
from rosbag2_py import (SequentialReader, SequentialWriter, StorageOptions,
                        ConverterOptions, TopicMetadata)
from rclpy.serialization import deserialize_message, serialize_message
from sensor_msgs.msg import PointCloud2
from nav_msgs.msg import Odometry
from builtin_interfaces.msg import Time

in_uri, out_uri = sys.argv[1], sys.argv[2]

RESTAMP = {"/lidar_points": PointCloud2, "/utlidar/robot_odom": Odometry}

reader = SequentialReader()
reader.open(StorageOptions(uri=in_uri, storage_id="sqlite3"),
            ConverterOptions("cdr", "cdr"))

# Mirror topic metadata into the writer.
topo = {t.name: t for t in reader.get_all_topics_and_types()}
writer = SequentialWriter()
writer.open(StorageOptions(uri=out_uri, storage_id="sqlite3"),
            ConverterOptions("cdr", "cdr"))
for name, meta in topo.items():
    writer.create_topic(TopicMetadata(
        name=name, type=meta.type,
        serialization_format=meta.serialization_format))

def ns_to_time(ns: int) -> Time:
    return Time(sec=ns // 1_000_000_000, nanosec=ns % 1_000_000_000)

n_re, n_pass = 0, 0
while reader.has_next():
    topic, data, t = reader.read_next()          # t = receive time (ns)
    if topic in RESTAMP:
        msg = deserialize_message(data, RESTAMP[topic])
        msg.header.stamp = ns_to_time(t)          # collapse to recv domain
        data = serialize_message(msg)
        n_re += 1
    else:
        n_pass += 1
    writer.write(topic, data, t)

print(f"re-stamped {n_re} msgs (/lidar_points + /utlidar/robot_odom), "
      f"passed through {n_pass}.")
print(f"output bag: {out_uri}")
