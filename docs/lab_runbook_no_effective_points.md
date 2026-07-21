# Lab Runbook — FAST-LIO2 "No Effective Points!"

**Sintomo:** all'avvio del container (`docker compose up --build -d`) appare una pointcloud
approssimata del laboratorio, poi dopo pochi secondi i log emettono ripetutamente
`No Effective Points!` e l'odometria diverge.

**Root cause (confermata da codice):** divergenza dell'ESIKF.
`laserMapping.cpp:747` stampa l'errore solo quando OGNI punto downsampled, proiettato
con la posa stimata corrente, non trova 5 vicini di mappa entro ~2.24 m
(`pointSearchSqDis>5`, riga 710) o fallisce il test di planarità (riga 722).
Non è sparsità a freddo: la mappa si semina (cloud iniziale ok), poi la posa predetta
esce dalla mappa. Traccia classica: **funziona da fermo, diverge in rotazione/movimento.**

Riferimenti NotebookLM (notebook "Unitree Go2 Docs"): paper FAST-LIO2 (back-propagation
§measurement model) + README ufficiale (time sync, extrinsic, `time_sync_en`).

---

## Decision tree (esegui in ordine, fermati al primo confermato)

### STEP 0 — Gating: la build include i fix del 2026-05-23?
```bash
git -C ~/Documents/Uni/Tesi/go2_ws log --oneline -3 -- src/fast_lio_ros2
# Devono comparire 4bc8fcc (PTP truncation) e 82c756e (IMU starvation + timestamp field)
```
- Se l'ultimo `--build` del container è **precedente** a quei commit → **H1**.
  Il fix #2 (campo per-punto `time` vs `timestamp` di Hesai) azzerava i timestamp
  per-punto → niente de-skew → smear sotto movimento → divergenza. **È quasi
  certamente la causa.** Azione: rebuild pulito e ritesta:
  ```bash
  cd ~/Documents/Uni/Tesi/go2_ws/docker
  docker compose down
  docker compose up --build -d
  docker compose logs -f go2_navigation
  ```
  Se l'errore sparisce → chiuso. Altrimenti prosegui.

### STEP 1 — Verifica che i timestamp per-punto siano popolati (H1 residuo)
```bash
# dentro il container o con ROS sourced
ros2 topic echo /lidar_points --field fields --once
# Deve esistere un campo 'timestamp' (FLOAT64). Nei log NON deve apparire
# "Failed to find match for field 'time'."
```
- Campo assente o warning presente → preprocess/driver non coerenti. Ricontrolla
  `preprocess.h:56` → `(double, time, timestamp)` e `config.yaml send_point_cloud_ros: true`.

### STEP 2 — IMU: unità giroscopio (H2, la più insidiosa)
```bash
# Tieni il cane FERMO, poi imbardalo dolcemente a mano
ros2 topic echo /utlidar/imu --once
```
Controlla `angular_velocity`:
- **rad/s (corretto):** in imbardata lenta \|ω\| ~ 0.5–2.0, max pochi rad/s.
- **deg/s (BUG):** valori nell'ordine di decine/centinaia.

`accel` (`linear_acceleration`) da fermo deve avere \|a\| ≈ 9.8 (m/s²). Nota: la
scala accel è auto-normalizzata in `IMU_Processing.hpp:193`, quindi NON è causa di
divergenza; il giroscopio invece è usato grezzo in `(gyr − bg)` → deg/s = ~57× →
esplode in rotazione.
- Se deg/s → serve nodo/repubblicazione con conversione `×π/180` su `angular_velocity`
  prima di FAST-LIO2 (oppure patch nel handler IMU).

### STEP 3 — Coerenza dominio di clock IMU vs LiDAR (H3)
```bash
ros2 topic echo /utlidar/imu  --field header.stamp --once
ros2 topic echo /lidar_points --field header.stamp --once
```
- I due stamp devono coincidere entro ~ms (entrambi su clock host wall-clock:
  LiDAR per `use_timestamp_type:1`, IMU se il bridge unitree usa `now()`).
- Divario di secondi/epoche diverse → `sync_packages` affama l'IMU → divergenza.
  Fix: ristampare l'IMU con clock host, o allineare la sorgente di clock.

Verifica anche che l'IMU arrivi a ~200 Hz:
```bash
ros2 topic hz /utlidar/imu      # atteso ~200
ros2 topic hz /lidar_points     # atteso ~10
```

### STEP 4 — Instrumentazione: osserva il collasso di effct_feat_num (H4)
Abilita il contatore di debug (richiede rebuild). Scommenta `laserMapping.cpp:1018`:
```cpp
cout<<"[ mapping ]: In num: "<<feats_undistort->points.size()<<" downsamp "
    <<feats_down_size<<" Map num: "<<featsFromMapNum<<"effect num:"<<effct_feat_num<<endl;
```
Rebuild e osserva: se `effect num` parte alto e decade verso 0 mentre `In/downsamp`
restano sani → conferma divergenza (non sparsità). Se `downsamp` è già <~50 →
sparsità reale → applica tuning STEP 5.

### STEP 5 — Tuning sparsità per LiDAR 16-line (H4, se confermato)
In `src/fast_lio_ros2/config/hesai_xt16.yaml` (valori NotebookLM per 16-beam):
```yaml
preprocess:
  point_filter_num: 2     # era 4 — tieni più punti grezzi
  blind: 0.3              # era 0.5 — comunque scarta ritorni dal corpo Go2
filter_size_surf: 0.2     # era 0.5 — feature più dense per 16 linee
filter_size_map:  0.3     # era 0.5 — ikd-Tree più densa
```
⚠️ Costo CPU maggiore sull'Orin — verifica che FAST-LIO2 stia in real-time (no lag crescente).

### STEP 5b — Inizializzazione EKF da fermo (H5)
All'avvio del nodo, tieni il cane **piatto e immobile per ~2 s**. La gravità viene
inizializzata mediando l'accelerometro (`IMU_Processing.hpp:182-193`); vibrazioni dei
motori in standing corrompono il vettore gravità → bias iniziale errato.

---

## Cattura log per analisi offline
```bash
docker compose logs --no-color go2_navigation > /tmp/fastlio_run_$(date +%H%M).log
# registra anche un bag breve per replay
ros2 bag record -o /tmp/lab_bag /utlidar/imu /lidar_points /Odometry /tf /tf_static
```
Porta il log + bag e ripartiamo dall'evidenza.

---

## Cosa NON è la causa (già escluso staticamente)
- **extrinsic**: `hesai_xt16.yaml` ha `extrinsic_T:[0.171,0,0.0908]`, `R=I₃`,
  `extrinsic_est_en:false` — coerente con matrice ufficiale Unitree.
- **time_sync_en**: già `false` (corretto: ci si affida al clock host/PTP, non al soft-sync).
- **scala accelerometro (g vs m/s²)**: irrilevante, normalizzata in init gravità.
