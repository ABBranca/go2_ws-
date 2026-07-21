---
type: concept
tags: [slam, ros2, workflow, config]
aliases: [Rosbag Replay Workflow, Offline SLAM Development, Record-Replay]
updated: 2026-06-01
---

# Offline SLAM Development (Rosbag Record / Replay)

## Overview

Workflow per sviluppare e tunare gli algoritmi SLAM **senza accesso fisico al
robot ad ogni iterazione**. Si registra una volta sul [[go2-robot]] lo stream
grezzo dei sensori in un rosbag, poi lo si rigioca a piacere su una macchina di
sviluppo (laptop). L'algoritmo SLAM è deterministico sull'input: lo stesso
stream stamp-allineato produce la stessa traiettoria e la stessa mappa ad ogni
replay. Il robot diventa quindi un semplice *produttore di stream*; una volta su
disco, l'[[orin-dock]] è ridondante per il lavoro algoritmico.

Questo è il pattern standard dello sviluppo SLAM (KITTI, Newer College, i
benchmark di FAST-LIO2/LIO-SAM nascono tutti così): record-once-replay-many.
La sessione di laboratorio del 2026-05-29 ha già usato questo metodo in forma
informale (bag `/tmp/standup_bag`, `/tmp/walk_bag`) per diagnosticare la
[[z-observability|divergenza Z di FAST-LIO2]].

---

## Principio cardine: registrare i produttori, non i prodotti

> Registra i topic **grezzi a monte**, mai quelli derivati dai nodi sotto test.

`/scan`, e le TF `map→odom` / `odom→base_link` sono **output dei nodi che stai
sviluppando**. Registrarli congelerebbe esattamente il comportamento che vuoi
ri-tunare. Registra invece gli input grezzi e **rigenera tutto il downstream ad
ogni replay**. Così puoi ri-tunare l'height-band di `pointcloud_to_laserscan` o
i parametri di [[slam-toolbox-2d]] contro dati sensore fissi.

### Topic da registrare

| Topic | Tipo | Ruolo | Consumato da |
|-------|------|-------|--------------|
| `/lidar_points` | `sensor_msgs/PointCloud2` | Point cloud grezza XT-16 (frame `hesai_lidar`) | `pointcloud_to_laserscan` → `/scan` |
| `/utlidar/robot_odom` | `nav_msgs/Odometry` | Odometria leg-kinematic Go2 (Z ancorato al suolo) | [[tf-tree\|odom_tf_broadcaster]] → TF `odom→base_link` |
| `/utlidar/imu` | `sensor_msgs/Imu` | IMU Go2 (~253 Hz), unica sorgente inerziale | [[fast-lio2]] (path deprecato) |
| `/tf_static` | `tf2_msgs/TFMessage` | Extrinsics statici `base_link→hesai_lidar` | tutti i nodi |

**Perché tutti e tre i topic dati**: `/utlidar/robot_odom` serve al pipeline 2D
corrente ([[slam-toolbox-2d]]); `/utlidar/imu` serve al path [[fast-lio2]]
deprecato. Registrandoli entrambi, **un solo bag rigioca per entrambi gli
esperimenti SLAM** — a prova di futuro contro un'eventuale inversione del pivot
2D.

**Da NON registrare**: `/tf` (dinamico, generato dai nodi sotto test), `/scan`,
`/map`. Solo `/tf_static` va catturato (è una fixture immutabile).

---

## Procedura

### 1. Sanity-check pre-registrazione (sul robot, stack attivo)

```bash
ros2 topic hz /lidar_points /utlidar/robot_odom /utlidar/imu
# atteso: ~10 Hz, ~50–250 Hz, ~253 Hz
ros2 topic echo /utlidar/imu --once          # stamp non-zero
ros2 topic echo /utlidar/robot_odom --once   # pose plausibile
```

### 2. Registrazione (sul [[go2-robot]] / [[orin-dock]])

```bash
ros2 bag record -o go2_slam_run1 \
  --storage mcap \
  --compression-mode file --compression-format zstd \
  /lidar_points /utlidar/robot_odom /utlidar/imu /tf_static
```

- `--storage mcap`: compressione + random-seek migliori di sqlite3 (default).
  `/lidar_points` domina la dimensione (~50–100 MB/min per XT a 16 linee).
- `zstd` per le run lunghe.

### 3. Replay (laptop di sviluppo)

```bash
# terminale 1 — pubblica /clock dagli stamp registrati
ros2 bag play go2_slam_run1 --clock

# terminale 2 — pipeline SLAM 2D corrente con sim time
ros2 launch go2_nav_bridge mapping_2d.launch.py use_sim_time:=true
```

> ⚠️ **`--clock` + `use_sim_time:=true` è la coppia critica.** `ros2 bag play`
> pubblica `/clock` dagli header stamp registrati; i nodi devono consumarlo o
> tagheranno wall-clock fresco in fase di processing → lookup TF fallisce
> ("extrapolation into the future/past"). [[slam-toolbox-2d]] e [[fast-lio2]]
> allineano i messaggi per header stamp, quindi la preservazione fedele degli
> stamp nel bag = risultato SLAM identico offline vs live. Vedi anche
> [[ptp-sync]] per la genesi degli stamp lato LiDAR.

Per iterare il tuning in continuo: `ros2 bag play ... --loop`.

> ⚠️ **Prerequisito nascosto: stamp in un solo dominio di clock.** La coppia
> `--clock`/`use_sim_time` presuppone che gli header stamp del bag siano
> **internamente coerenti**. Sui bag registrati finora NON lo erano:
> `/lidar_points` è taggato col system clock dell'[[orin-dock]] (non-NTP-synced)
> mentre `/utlidar/robot_odom` usa il clock UTC dell'MCU → gap ~56 anni →
> [[slam-toolbox-2d|slam_toolbox]] scarta il 100 % degli scan. Vedi
> [[clock-domain-mismatch]] per diagnosi (`bags/dump_stamps.py`) e fix
> (`bags/restamp_bag.py`, oppure sync del clock Orin a monte). Da escludere
> **prima** di ogni altra diagnosi quando un bag nuovo droppa tutti gli scan.

Per il tuning post-fix dei parametri 2D (Huber, scan_buffer, resolution,
levelling, ring-select) e i risultati misurati: [[2d-map-tuning]]. Harness:
`bags/run_2d_replay_test.sh <bag> <out>` + `bags/map_stats.py`.

---

## Valore metodologico

- **Disaccoppia i domini di guasto.** Problemi hw/rete/DDS-discovery (la saga
  `cyclonedds.xml`/eth0, vedi [[dds-config]]) spariscono in replay: bug residuo
  = bug algoritmico puro. Root-cause più rapido.
- **Abilita il regression testing.** Un bag canonico = fixture. Dopo ogni
  modifica a `pointcloud_to_laserscan` o ai param SLAM, replay + diff della
  traiettoria → cattura regressioni silenziose. Stesso principio di uno unit
  test, applicato al SLAM.
- **Matrice di test offline.** Registra bag rappresentativi — rettilineo,
  loop-closure, rotazione rapida, corridoio feature-poor, stand-up — per coprire
  i casi limite senza ri-portare il cane in campo.

### Caveat: il gap sim-to-real è mono-direzionale

Il replay valida la **correttezza algoritmica**, non la **performance
real-time**. Il laptop processa più veloce/lento dell'[[orin-dock]]; bug di
timing CPU-bound (es. la IMU starvation risolta in `82c756e`) possono nascondersi
in replay. La validazione finale di timing richiede comunque il robot fisico.

---

## Pagine correlate

- [[slam-toolbox-2d]] — SLAM 2D corrente, consumatore di `/scan` + odom
- [[fast-lio2]] — path LiDAR-inertial deprecato, consumatore di `/utlidar/imu`
- [[tf-tree]] — catena frame e ruolo di `odom_tf_broadcaster`
- [[hesai-xt16]] — sorgente di `/lidar_points`
- [[imu]] — sorgente `/utlidar/imu`
- [[ptp-sync]] — sincronizzazione clock / genesi degli stamp
- [[clock-domain-mismatch]] — difetto di stamp multi-dominio che rompe il replay; fix re-stamp
- [[2d-map-tuning]] — tuning parametri 2D + risultati misurati su bag re-stamped
- [[z-observability]] — diagnosi che ha usato per prima questo workflow

<!-- updated 2026-06-01: pagina creata — workflow record/replay rosbag per sviluppo SLAM offline -->
<!-- updated 2026-06-03: aggiunto prerequisito clock-domain (mixed-stamp bag) + link 2d-map-tuning -->
