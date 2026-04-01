# Networking & DDS — Unitree Go2

## Topologia di Rete

| Unità | IP | SSH | Ruolo |
|-------|----|----|-------|
| MCU (Unitree Board) | `192.168.123.161` | **Bloccato** | Motori, SDK, adattatore Wi-Fi |
| Expansion Dock (Orin) | `192.168.123.18` | Abilitato | Stack ROS 2 primario |
| Laptop sviluppatore | `192.168.123.10` | — | Ethernet RJ45 al Dock |
| Hesai XT16 LiDAR | `192.168.123.20` | — | UDP `2368`, TCP `9347` |

**Problema critico:** Il MCU non fa bridging del traffico DDS dall'Ethernet interno al Wi-Fi.
SSH sul MCU è bloccato → impossibile configurare IP forwarding/NAT su di esso.

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
  - ROS_DOMAIN_ID=1
volumes:
  - ./cyclonedds.xml:/config/cyclonedds.xml:ro
```

**Senza questa config:** CycloneDDS seleziona l'interfaccia casualmente → fallimenti intermittenti nel node discovery.

## Connettività Laptop → Orin → MCU

### Percorso attuale (Ethernet diretto)

```
Laptop (123.10) ──Ethernet──▶ Orin Dock (123.18) ──Interno──▶ MCU (123.161)
```

Il Dock comunica con il MCU via SDK Unitree. Il laptop vede i topic ROS 2 pubblicati nel container Docker sul Dock.

### Estensione Wi-Fi (opzione 1 — dongle USB)

Dongle raccomandato: **Alfa AWUS036ACM** (MediaTek MT7612U, driver nativo Linux, compatibile ARM64).

```bash
# Abilitare IP forwarding sul Dock dopo connessione Wi-Fi (ARSCONTROL)
sudo sysctl -w net.ipv4.ip_forward=1
sudo iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
```

### Estensione Wi-Fi (opzione 2 — travel router)

GL.iNet connesso alla porta Ethernet Go2: bridging L2 trasparente, nessun problema di driver.

### Verifica DDS Discovery

```bash
# Sul laptop (DOMAIN_ID=1):
ros2 node list

# Se vuoto, usare unicast explicit peers in cyclonedds.xml:
# <Discovery><Peers><Peer Address="192.168.123.18"/></Peers></Discovery>
```

## Latenza e Banda

Se RViz2 è lento con `/lidar_points`:
- Usare un nodo filtro per ridurre la frequenza del topic
- Oppure `ros2 topic pub` con `pcl/PCLPointCloud2` compresso

## Diagnostica Connettività

```bash
# 1. Raggiungibilità base
ping 192.168.123.18

# 2. Docker attivo
ssh unitree@192.168.123.18 "docker ps"

# 3. Topic visibili dall'esterno
ros2 topic list   # dalla macchina laptop

# 4. Report completo DDS
ros2 doctor --report
```
