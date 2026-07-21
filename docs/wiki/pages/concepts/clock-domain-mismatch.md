---
type: concept
tags: [slam, ros2, workflow, imu, sensor]
aliases: [clock domain mismatch, bag timestamp mismatch, mixed clock bag, re-stamp, restamp]
updated: 2026-06-03
---

# Clock-Domain Mismatch in Recorded Bags

A recording defect that silently breaks 2D SLAM ([[slam-toolbox-2d]]) on otherwise-good
data: the two sensor streams carry `header.stamp` values from **two different clocks**.

## Symptom (lab 2026-06-03)
Replaying bag `slam2d_19700101_102247` through [[offline-slam-replay|the 2D replay launch]]:
[[slam-toolbox-2d|slam_toolbox]] drops **100 % of scans** —

```
Message Filter dropping message: frame 'base_link' at time 8568.704 for reason
'the timestamp on the message is earlier than all the data in the transform cache'
```

→ no `map→odom`, no `/map`, `map_saver` times out. The launch, frames, topics, and rates
are all correct; nothing in the pipeline is misconfigured.

## Root cause — two publishers, two clocks
The bag's header stamps live in disjoint domains:

| Topic | `header.stamp` | Clock | Domain |
|-------|---------------|-------|--------|
| `/lidar_points` | ≈ **8 568.6 s** | [[orin-dock]] system clock, **NTP-unsynced at boot** | "1970" epoch |
| `/utlidar/robot_odom` | ≈ **1 780 508 524 s** | Go2 MCU firmware RTC, synced UTC | 2026 |

Gap ≈ **+1.78×10⁹ s (~56 years)**. The Hesai ROS driver stamps clouds with the Orin
`system_clock` (which had not yet NTP-synced — the same "1970 epoch" effect noted on the
[[hesai-xt16]] driver); the MCU stamps `/utlidar/robot_odom` from its own synced clock.

`ros2 bag play --clock` publishes `/clock` from **receive** time (one domain, ≈8568 s), so
`/scan` inherits the cloud stamp (~8568) while `odom_tf_broadcaster` faithfully copies the
odom's 2026 stamp into the `odom→base_link` TF. The TF cache is therefore all-2026; the
8568-stamped scan is "earlier than all data" → dropped. This is **not** the
`odom_tf_broadcaster` design's fault (it correctly propagates `msg.header.stamp`); it is a
bag-level defect.

> Distinct from [[ptp-sync]]: PTP aligns the **LiDAR↔IMU** clocks for [[motion-undistortion]].
> This mismatch is **cloud↔leg-odom** and originates from the Orin system clock being
> unsynced when the driver stamped, independent of the LiDAR's own PTP state.

## Diagnosis
`bags/dump_stamps.py <bag>` prints per-topic `header` vs `recv` stamp and the cross-topic
domain gap. A skew of `recv − header ≈ 0` on one topic and ≈ −1.78e9 on the other is the
fingerprint.

## Fix
- **Proper (record-time):** sync the Orin clock **before** recording — chrony/NTP, or PTP-
  slave to the MCU — so the Hesai driver and firmware stamp in one domain. Then no
  post-processing is needed. See [[ptp-sync]].
- **Offline salvage:** `bags/restamp_bag.py <in> <out>` rewrites **both** topics'
  `header.stamp = receive-time`, collapsing them to the single record-clock domain. Validated:
  `slam2d_19700101_102247` → `slam2d_timefixed` → **0 drops**, coherent 8.8 × 16.5 m map,
  `map→odom` yaw correction −17° (healthy leg-odom drift correction). The sensor *data* was
  sound; only the stamps were broken.

## Why it matters
The [[offline-slam-replay]] `--clock` + `use_sim_time` caveat assumes the bag's stamps are
**internally consistent**. This defect violates that assumption *before* replay even starts —
a class of failure to rule out first when a fresh bag drops every scan. Related tuning after
the fix: [[2d-map-tuning]].

<!-- created 2026-06-03: lab finding — cloud/leg-odom clock-domain split breaks 2D SLAM; re-stamp fix -->
