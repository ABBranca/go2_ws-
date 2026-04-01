# FAST-LIO2 — Tuning per Go2 + Hesai XT16

## Configurazione LiDAR (Hesai XT16)

| Parametro YAML | Valore | Note |
|----------------|--------|------|
| `lidar_type` | `2` | Hesai/Velodyne/Ouster |
| `N_SCANS` | `16` | Canali verticali XT16 |
| `filter_size_surf` | `0.3` | Voxel downsampling superficie |
| `filter_size_map` | `0.3` | Voxel downsampling mappa |
| Frequenza scan | 10 Hz | Preferita per densità (20 Hz per reattività) |
| FOV orizzontale | 360° | |
| FOV verticale | ±15° | |

## Estrinseci Ufficiali Unitree (T_base_link_hesai_lidar)

```yaml
extrinsic_T: [0.171, 0.0, 0.0908]   # metri: x=avanti, y=sinistra, z=su
extrinsic_R: [1,0,0, 0,1,0, 0,0,1]  # identità: nessuna rotazione relativa
```

**Verifica con calibro:** tolleranza target ±2 mm. Misurare dal centro IMU (corpo Go2) al centro LiDAR.

## Parametri Rumore IMU (BMI088)

Stime iniziali da datasheet — da affinare con Allan Variance:

```yaml
gyr_cov:   0.0001    # varianza rumore giroscopio  (rad/s)^2/Hz
acc_cov:   0.001     # varianza rumore accelerometro (m/s^2)^2/Hz
b_gyr_cov: 0.00001   # varianza bias giroscopio
b_acc_cov: 0.0001    # varianza bias accelerometro
```

### Procedura Allan Variance (5 min static rosbag)

```bash
# 1. Robot completamente fermo su superficie rigida
# 2. Registrare:
ros2 bag record /imu/data -o imu_calibration_$(date +%Y%m%d)

# 3. Analizzare offline con imu_utils o allan_variance_ros
# 4. Aggiornare hesai_xt16.yaml con valori ricavati
```

Segni di parametri IMU sbagliati: drift Z in ambienti piatti, oscillazioni nella stima di orientazione.

## Avvio e Monitoraggio

```bash
# Avvio
ros2 launch fast_lio mapping.launch.py config_file:=hesai_xt16.yaml rviz:=true

# Monitoraggio odometria IEKF
ros2 topic echo /odometry/imu_incremental

# Verifica frequenza point cloud
ros2 topic hz /lidar_points
```

## Tuning Avanzato

| Problema | Parametro da modificare |
|----------|------------------------|
| Mappa troppo sparsa | Ridurre `filter_size_surf` e `filter_size_map` |
| CPU troppo alta | Aumentare `filter_size_surf` (meno punti) |
| Drift in corridoi | Verificare estrinseci; considerare loop closure esterno |
| SLAM instabile a bassa velocità | Verificare `acc_cov` — rumore accelerometro sovrastimato |
