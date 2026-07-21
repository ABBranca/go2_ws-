---
type: concept
tags: [hardware, sensor, timing, ros2]
aliases: [PTP, IEEE 1588, PTP grandmaster, ptp4l]
updated: 2026-05-26
---

# PTP Synchronization — LiDAR Clock Sync

## Scopo

[[hesai-xt16]] usa timestamp PTP assoluti (µs). Senza un grandmaster PTP attivo sulla rete, il LiDAR entra in stato **Free Run** (orologio interno libero, drift rispetto al ROS clock dell'[[orin-dock]]). Questo causa timestamp fuori sync tra point cloud e IMU (`/utlidar/imu`) → IMU starvation in [[fast-lio2]].

## Stati PTP (Pandar Console → Home)

| Stato | Significato |
|-------|-------------|
| **Free Run** | Nessun grandmaster trovato. Clock LiDAR oscilla liberamente |
| **Tracking** | Grandmaster trovato, servo PI in convergenza (offset decrescente) |
| **Lock** | Clock sincronizzato. Offset residuo stabile |

## Orin come PTP Grandmaster

[[orin-dock]] deve girare `ptp4l` come master sul segmento `192.168.123.x`.

**Nota hardware:** `eth0` dell'Orin (L4T) non espone PHC (PTP Hardware Clock) via `ethtool` → richiede software timestamping (`-S`).

### Comando manuale

```bash
sudo ptp4l -i eth0 -m -S
```

- `-S` — software timestamping (obbligatorio su Orin L4T)
- `-m` — log su stdout
- senza `-s` — BMCA elegge Orin grandmaster automaticamente (nessun altro master su rete isolata Go2)

### Integrazione host (architettura corrente)

`ptp4l` gira direttamente sull'**host Orin** (fuori dal container), non nel container Docker.

**Motivazione:** avere due istanze `ptp4l` sullo stesso `eth0` fisico (host + container) causa conflitto BMCA — entrambe si candidano come grandmaster. Architettura pulita: un solo master sull'interfaccia fisica.

```bash
# Host Orin — avvio manuale o systemd unit
sudo ptp4l -i eth0 -m -S
```

`linuxptp` **rimosso** da `docker/Dockerfile` (commit recente). `docker/ros_entrypoint.sh` non lancia più `ptp4l`.

Il container eredita la sincronizzazione PTP del kernel host tramite clock di sistema — nessuna partecipazione diretta al dominio PTP IEEE 1588.

## Accuratezza con SW Timestamping

Offset residuo dopo Lock con software timestamping: ~10–50 µs. Accettabile per SLAM:

- Periodo IMU (`/utlidar/imu` @ 253 Hz) ≈ 4 ms
- Errore di sync (50 µs) ≈ 1.25% del periodo IMU → trascurabile per [[motion-undistortion]]

## Parametro Driver Correlato

`src/hesai_ros_driver_2/config/config.yaml`:

```yaml
use_timestamp_type: 0  # usa timestamp del point cloud (PTP), non receive timestamp
```

Con PTP in Free Run, questo campo causa timestamps arbitrari. Alternativa di emergenza: `use_timestamp_type: 1` (receive timestamp = ROS clock host) — elimina dipendenza PTP ma introduce jitter CPU/rete.

## Oscillazione LOCKED ↔ TRACKING — Analisi

### Cause

- **SW timestamping varianza alta**: `-S` flag usa kernel timestamp su ingresso pacchetto. Jitter di rete su `eth0` (stessa interfaccia traffico LiDAR 10 Hz + DDS) fa uscire il servo PI dal threshold di Lock.
- **Master clock non disciplinato**: `CLOCK_REALTIME` dell'Orin senza NTP/GPS deriva liberamente. Il servo del LiDAR insegue un riferimento mobile → esce dal threshold periodicamente.

### Impatto su FAST-LIO2

| Offset durante TRACKING | Effetto |
|--------------------------|---------|
| < 1 ms | Trascurabile. Buffer IMU di FAST-LIO2 tollera ~5 ms |
| 1–5 ms | Undistortion degradata. Errore crescente in rotazioni veloci |
| > 5 ms | IMU starvation possibile → SLAM diverge |

Lo stato PTP (LOCKED/TRACKING) non è il problema — lo è il **valore dell'offset in µs durante TRACKING**.

### Diagnostica

```bash
sudo ptp4l -i eth0 -m -S 2>&1 | grep "master offset"
# master offset  [valore µs]  s2  freq ...
# s2 = LOCKED, s1 = TRACKING
```

Monitorare il valore numerico durante `s1` — se < 1000 µs, impatto SLAM trascurabile.

### Fix: disciplinare CLOCK_REALTIME dell'Orin

Radice del problema: master clock (Orin CLOCK_REALTIME) senza riferimento esterno. Soluzione:

```bash
# Orin host — abilita NTP (anche senza internet, fallback stratum locale)
sudo timedatectl set-ntp true
# oppure chrony con local stratum 10
```

Con master clock stabile, il servo del LiDAR converge e rimane in Lock.

## Riferimenti

- [[hesai-xt16]] — configurazione Pandar Console e timestamp handling
- [[fast-lio2]] — consumer dei timestamp LiDAR
- [[motion-undistortion]] — dipende da timestamp per-point corretti
