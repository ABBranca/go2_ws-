---
type: config
tags: [config, ros2, dds, networking]
aliases: [DDS, CycloneDDS, cyclonedds.xml]
sources: [cyclonedds.xml, cyclonedds_laptop.xml]
updated: 2026-07-22
---

# DDS Configuration — CycloneDDS

Full file: `docker/cyclonedds.xml`

## Design: one canonical file per role, both transport-agnostic

Two links between laptop and robot, one profile each:

- **Wired** — laptop `eno1` (`192.168.123.222`) ↔ robot `eth0` (`192.168.123.18`)
- **Wi-Fi AP** — laptop `wlan` (`10.42.0.x`) ↔ robot `wlan0` (`10.42.0.1`, NetworkManager shared-AP on the rtl8812au dongle)

| File | Role | Consumers |
|------|------|-----------|
| `docker/cyclonedds.xml` | Robot ([[orin-dock]], `network_mode: host`) | docker-compose, dev-deploy.sh, deploy_to_robot.sh |
| `cyclonedds_laptop.xml` (repo root) | Laptop / devcontainer | `scripts/setup_laptop_env.sh`, `.devcontainer/devcontainer.json` |

## Robot — `docker/cyclonedds.xml`

Multi-homes **both** NICs to serve either subnet simultaneously:

```xml
<NetworkInterface name="eth0"  priority="default" multicast="default" />
<NetworkInterface name="wlan0" priority="default" multicast="default" presence_required="false" />
```

`presence_required="false"` on `wlan0` lets the stack start when the USB dongle is absent (wired-only sessions). The robot **cannot** use `autodetermine` — with host networking it must bind both interfaces to reach a laptop on either subnet.

Robot discovery peers = the laptop's two addresses:

| Address | Role |
|---------|------|
| `127.0.0.1` | Loopback (local processes) |
| `192.168.123.222` | Laptop over wired link |
| `10.42.0.66` | Laptop over the Wi-Fi AP |

## Laptop — `cyclonedds_laptop.xml`

```xml
<NetworkInterface autodetermine="true" />
```

`autodetermine` binds whichever NIC has a route to the peers, so the hardware-specific name (`wlp3s0`/`eno1`) is never hardcoded. Only one transport is active at a time, so single auto-selection is exact.

Laptop discovery peers = the robot's two addresses:

| Address | Role |
|---------|------|
| `127.0.0.1` | Loopback |
| `192.168.123.18` | [[orin-dock]] over wired (`eth0`) |
| `10.42.0.1` | [[orin-dock]] over the Wi-Fi AP (`wlan0`) |

Unicast peers make discovery independent of multicast SPDP (many APs, incl. NM shared-mode, do not forward it). An unreachable peer is silently ignored, so the same file works on both links. `MaxAutoParticipantIndex=128` on both ends so unicast discovery reaches the >10-participant stack.

## Performance

```xml
<SocketReceiveBufferSize min="4 MiB"/>
```

Requires on host:
```bash
sudo sysctl -w net.core.rmem_max=4194304
```

Needed for high-bandwidth PointCloud2 traffic from [[hesai-xt16]] (10 Hz × ~60k points).

## Multicast

`AllowMulticast: true` — required for ROS 2 node discovery within subnet `192.168.123.0/24`.

## ⚠️ Common Failure Modes

- **Laptop sees only local topics, not the robot's.** Usual cause: DDS env not sourced → laptop falls back to FastDDS (Humble default) while the robot runs CycloneDDS. Fix: `source scripts/setup_laptop_env.sh` (sets `RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`, `ROS_DOMAIN_ID=0`, `ROS_LOCALHOST_ONLY=0`, `CYCLONEDDS_URI`), then `ros2 daemon stop && ros2 daemon start`.
- **Only low-index nodes visible** (e.g. Hesai lidar/imu but not Nav2/slam): `MaxAutoParticipantIndex` too low for unicast discovery of a >10-participant stack. Both files use `128`.
- **Wrong interface inside Docker**: without explicit binding the container may pick a virtual bridge → no discovery. Robot pins `eth0`+`wlan0`; laptop uses `autodetermine`.
- **AP drops multicast SPDP**: discovery over Wi-Fi relies on the unicast `<Peer>` entries; if it still fails, add an mDNS hostname peer (`<Peer address="orin.local"/>`).
