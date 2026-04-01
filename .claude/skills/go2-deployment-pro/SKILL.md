---
name: go2-deployment-pro
description: >
  Expert DevOps for Unitree Go2: Docker builds, ARM64 cross-compilation, rsync sync,
  CycloneDDS configuration, and deployment checklist. Trigger when the user asks about
  building, syncing, deploying, Docker, Dockerfile, colcon, or container issues on the robot.
tools: Bash, Read, Edit, Glob, Grep
---

# Go2 Deployment Pro

DevOps ottimizzato per il ciclo laptop (x86_64) → robot Orin Dock (ARM64, `192.168.123.18`).

## Ciclo Standard

```
Edita localmente → sync_to_dog.sh → build nel container → visualizza su laptop
```

### 1. Sync del codice (Ethernet, laptop su `192.168.123.10`)

```bash
./sync_to_dog.sh
# rsync -avz --exclude 'build/' --exclude 'install/' --exclude 'log/' src/ unitree@192.168.123.18:~/go2_ws/src/
```

### 2. Build nel container Docker

```bash
ssh unitree@192.168.123.18 "docker exec go2_navigation bash -c 'colcon build --symlink-install && source install/setup.bash'"

# Build package singolo:
docker exec go2_navigation bash -c 'colcon build --symlink-install --packages-select go2_nav_bridge'
```

**Nota critica:** `--symlink-install` fa symlink solo per `ament_cmake`. Per `ament_python` i file YAML/launch richiedono rebuild. Tieni i package di navigazione come `ament_cmake`.

### 3. Visualizzazione remota (laptop)

```bash
source /opt/ros/humble/setup.bash
export ROS_DOMAIN_ID=1
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
```

---

## Docker Lifecycle

### Avvio iniziale (sul robot)

```bash
cd docker && docker compose up --build -d
```

### Accesso al container

```bash
ssh unitree@192.168.123.18
docker exec -it go2_navigation bash
```

### Container che esce subito (Exit Code 0)

**Causa:** `CMD ["/bin/bash"]` senza TTY interattivo.
**Fix:** Avviare sempre con `-itd`:

```bash
docker run -itd --name go2_navigation <image>
```

---

## Dockerfile — Dipendenze Obbligatorie

Aggiungere sempre nel Dockerfile per evitare fallimenti noti:

```dockerfile
RUN apt-get update && apt-get install -y \
    libpcl-dev \
    ros-humble-pcl-ros \
    ros-humble-diagnostic-updater \
    && rm -rf /var/lib/apt/lists/*
```

Senza `libpcl-dev`: `livox_ros_driver2` e `fast_lio_ros2` falliscono durante `colcon build`.

---

## Build Immagine ARM64 Finale (produzione)

```bash
# Build multi-platform dal laptop
docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load .

# Trasferire al robot
docker save go2_nav_stack:latest | ssh -C unitree@192.168.123.18 'docker load'
```

Usare solo quando lo sviluppo è completo. Durante sviluppo, preferire build nativa su Orin.

---

## CycloneDDS — Configurazione Corretta

Creare `docker/cyclonedds.xml`:

```xml
<CycloneDDS>
  <Domain>
    <General>
      <NetworkInterface name="eth0" />
    </General>
  </Domain>
</CycloneDDS>
```

Montare in `docker-compose.yml`:

```yaml
environment:
  - CYCLONEDDS_URI=file:///config/cyclonedds.xml
volumes:
  - ./cyclonedds.xml:/config/cyclonedds.xml:ro
```

**Problema senza config esplicita:** CycloneDDS sceglie l'interfaccia casualmente → fallimenti intermittenti nel discovery dei nodi.

---

## Troubleshooting Build

### `livox_ros_driver2` — package.xml mancante

```bash
# Fix: symlink del file corretto
cd src/livox_ros_driver2
ln -s package_ROS2.xml package.xml
```

### submoduli non inizializzati

```bash
git submodule update --init --recursive
```

---

## Deployment Checklist

Eseguire in ordine prima di ogni sessione:

1. **Source:** `source install/setup.bash`
2. **Network:** `ros2 doctor --report` — verificare che CycloneDDS usi `eth0`
3. **Hardware:** `ping 192.168.123.20` — porta LiDAR UDP `2368` raggiungibile dal Dock
4. **Bridge:** `ros2 topic hz /cmd_vel` — frequenza ≥ 20 Hz
5. **Safety:** Verificare `mcp_watchdog` attivo prima di lanciare goal Nav2

---

## ROS_DOMAIN_ID

| Nodo | Valore |
|------|--------|
| Container Docker (Orin) | `1` |
| Laptop (visualizzazione) | `1` |
