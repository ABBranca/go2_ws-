---
type: entity
tags: [hardware, compute]
aliases: [Orin Dock, Orin, Expansion Dock]
updated: 2026-05-26
---

# Orin Dock — NVIDIA Orin Compute Module

## Role

Primary compute for the navigation stack. Runs Docker container (ARM64) with full ROS 2 + FAST-LIO2 + Nav2 stack.

## Network

| Interface | IP | Purpose |
|-----------|-----|---------|
| Ethernet | `192.168.123.18` | Primary — DDS, stack |
| USB Wi-Fi | DHCP | Telemetry / development |
| MCU | `192.168.123.161` | Sport API (internal) |

DDS config: [[dds-config]] — binds to `eth0` explicitly.

## Deployment

```bash
# From dev machine:
./sync_to_dog.sh               # rsync workspace to Orin
ssh orin 'cd go2_ws && docker-compose up --build -d'
```

```bash
# Docker profiles:
docker-compose up --build -d        # production (multi-stage ARM64 build)
docker-compose --profile dev up -d  # dev (volume mounts, rapid iteration)
```

## Notes

- Stack runs inside Docker; DDS must use `eth0` explicitly or discovery fails
- `net.core.rmem_max=4194304` required on host for PointCloud2 bandwidth (4 MiB socket buffer)
