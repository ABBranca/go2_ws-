---
name: go2-hardware-expert
description: >
  Expert calibration, diagnostics, and hardware procedures for Unitree Go2. Covers
  LiDAR-IMU extrinsics (Hesai XT16 + BMI088), TF verification, Orin thermals, battery
  monitoring, and physical inspection. Trigger when the user asks about calibration,
  extrinsics, IMU noise, sensor health, temperature, battery, or physical mounting.
tools: Bash, Read
---

# Go2 Hardware Expert

Procedure specializzate per calibrazione, diagnostica e manutenzione dell'hardware Go2,
con focus sull'accuratezza SLAM.

---

## Estrinseci LiDAR-IMU (Hesai XT16 su Go2)

### Valori Ufficiali Unitree (verificati)

| Parametro | Valore |
|-----------|--------|
| Traslazione T (base_link → hesai_lidar) | `[0.171, 0.0, 0.0908]` m |
| Rotazione R | Identità `I₃` (nessuna rotazione relativa) |

Fonte: documentazione ufficiale Unitree. Da verificare fisicamente con calibro (tolleranza ±2 mm).

### Verifica TF live

```bash
# Dentro il container Docker
ros2 run tf2_ros tf2_echo base_link hesai_lidar
```

### Pubblicazione TF statico (sintassi Humble)

```bash
ros2 run tf2_ros static_transform_publisher \
  --x 0.171 --y 0.0 --z 0.0908 \
  --yaw 0 --pitch 0 --roll 0 \
  --frame-id base_link --child-frame-id hesai_lidar
```

**Nota:** Pubblicare solo su `/tf_static`, mai periodicamente su `/tf`. Il nome frame deve corrispondere a `ros_frame_id: hesai_lidar` in `hesai_ros_driver_2/config/config.yaml`.

---

## Parametri Rumore IMU (BMI088)

Stime iniziali da datasheet per `src/fast_lio_ros2/config/hesai_xt16.yaml`:

```yaml
gyr_cov:   0.0001    # (rad/s)^2/Hz
acc_cov:   0.001     # (m/s^2)^2/Hz
b_gyr_cov: 0.00001   # bias gyro
b_acc_cov: 0.0001    # bias accelerometro
```

### Procedura di Caratterizzazione (Allan Variance)

Se FAST-LIO2 deriva, caratterizzare il rumore reale:

1. Posizionare il robot **completamente fermo** su superficie rigida
2. Registrare rosbag di 5 minuti:
   ```bash
   ros2 bag record /imu/data -o imu_static_5min
   ```
3. Analizzare con `imu_utils` o `allan_variance_ros`
4. Aggiornare `hesai_xt16.yaml` con i valori ricavati

---

## Ispezione Fisica

### Montaggio LiDAR

- Verificare che il Hesai XT16 sia **livellato**. Un'inclinazione di 1° causa errori di mapping significativi a 10 m di distanza.
- Controllare il cavo Ethernet del LiDAR: `ping 192.168.123.20`

### Temperatura Orin (Throttling termico)

Monitorare durante operazioni intensive FAST-LIO2 + Nav2:

```bash
# Sul robot (Orin)
ssh unitree@192.168.123.18 "tegrastats"
```

| Temperatura | Stato |
|-------------|-------|
| < 75°C | Nominale |
| 75-85°C | Attenzione |
| > 85°C | **Throttling** — FAST-LIO2 rallenta |

Se il throttling causa problemi SLAM: ridurre `scan_line` o aumentare `filter_size_surf`.

---

## Diagnostica Batteria e MCU

### Livelli Batteria (`SportModeState`)

```bash
ros2 topic echo /utapi/sport_state
```

| SoC | Azione |
|-----|--------|
| 20% | Warning — rientrare in base |
| 10% | **Critico** — RTH immediato |

### Salute MCU

```bash
# Frequenza aggiornamento stato sport
ros2 topic hz /utapi/sport_state
```

Assenza di aggiornamenti per > 0.5 s → problema di networking interno (MCU non raggiungibile).

### Bridge go2_nav_bridge

```bash
# Verificare che il bridge riceva cmd_vel a ≥ 20 Hz
ros2 topic hz /cmd_vel
```

---

## Topologia di Rete Hardware

| Unità | IP | SSH | Ruolo |
|-------|----|----|-------|
| MCU (Motion Control) | `192.168.123.161` | **Bloccato** | Motori, SDK, adattatore Wi-Fi |
| Expansion Dock (Orin) | `192.168.123.18` | Abilitato | Docker + stack ROS 2 |
| Laptop sviluppatore | `192.168.123.10` | — | Editing, Rviz2 |
| Hesai XT16 LiDAR | `192.168.123.20` | — | UDP `2368` dati, TCP `9347` PTC |

**Nota critica:** Il MCU **non** fa bridging del traffico DDS dall'Ethernet interno al suo adattatore Wi-Fi. SSH sul MCU è bloccato → impossibile configurare IP forwarding/NAT su di esso.

---

## Configurazione Hesai XT16

| Parametro | Valore |
|-----------|--------|
| Canali (N_SCANS) | 16 |
| FOV orizzontale | 360° |
| FOV verticale | ±15° |
| Frequenza scan consigliata | 10 Hz (maggiore densità di punti) |
| Porta UDP | `2368` |
| Porta TCP PTC | `9347` |
