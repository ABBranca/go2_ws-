---
type: config
tags: [config, ros2, dds, networking]
aliases: [DDS, CycloneDDS, cyclonedds.xml]
sources: [cyclonedds.xml]
updated: 2026-05-26
---

# DDS Configuration — CycloneDDS

Full file: `docker/cyclonedds.xml`

## Network Interface

```xml
<NetworkInterface name="eth0" priority="default" multicast="default" />
```

Binds explicitly to `eth0`. **Critical** — without this, DDS uses wrong interface inside Docker → discovery fails.

## Discovery Peers

| Address | Role |
|---------|------|
| `127.0.0.1` | Loopback (local processes) |
| `192.168.123.18` | [[orin-dock]] (stack) |
| `192.168.123.222` | Developer laptop (replace with actual IP) |

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

## ⚠️ Common Failure Mode

If DDS is not explicitly configured to use `eth0`, Docker container may bind to virtual bridge interface → nodes on Orin Dock and developer machine cannot discover each other. Symptom: topics visible locally but not on remote machine.
