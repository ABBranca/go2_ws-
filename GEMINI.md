# Unitree Go2 ROS 2 Navigation Workspace

## Project Overview
This workspace is designed to enable autonomous navigation for the Unitree Go2 quadruped robot using **ROS 2 Humble**. It integrates the Unitree Go2 SDK, the `unitree_ros2` package, and `FAST_LIO2` for LiDAR-based SLAM with the Hesai XT16 sensor.

### Core Technologies
*   **Robot:** Unitree Go2
*   **LiDAR:** Hesai XT16 (16-channel)
*   **ROS 2 Version:** Humble
*   **SLAM:** [FAST_LIO2 (ROS 2)](https://github.com/hku-mars/FAST_LIO/tree/ROS2)
*   **Navigation:** Nav2
*   **Deployment:** Docker (Onboard computer)
*   **Visualization:** Rviz2 (Remote laptop via Ethernet)

## Architecture
The system consists of the following components:
1.  **Unitree ROS 2 Interface:** Communicates with the robot's proprietary SDK for motion control and state feedback.
2.  **FAST_LIO2:** Processes point clouds from the Hesai XT16 to provide high-frequency odometry and local mapping.
3.  **Nav2 Bridge (`go2_nav_bridge`):** A custom node that translates standard `geometry_msgs/Twist` commands from Nav2 into `unitree_go/msg/Go2Command` (or equivalent SDK messages) to control the robot's locomotion.
4.  **Nav2 Stack:** Handles path planning, obstacle avoidance, and goal management.

## Project Structure
```text
go2_ws/
├── docker/                 # Docker configuration for onboard deployment
│   ├── Dockerfile          # ROS 2 Humble base image with Nav2
│   └── docker-compose.yml  # Manages the navigation container
├── src/
│   ├── go2_nav_bridge/     # ROS 2 package for Nav2-to-SDK translation
│   │   ├── src/bridge_node.cpp
│   │   ├── CMakeLists.txt
│   │   └── package.xml
│   ├── unitree_ros2/       # (TODO) Clone Unitree Go2 ROS 2 SDK here
│   └── fast_lio_ros2/      # (TODO) Clone FAST_LIO2 ROS 2 version here
└── GEMINI.md               # This documentation
```

## FAST-LIO2 Documentation
**FAST-LIO2** (Fast LiDAR-Inertial Odometry) è un framework di odometria e mapping LiDAR-inerziale veloce, robusto e versatile.

### Caratteristiche Principali
*   **ikd-Tree:** Utilizza una struttura dati "incremental k-d tree" dinamica che permette l'inserimento di nuovi punti e la rimozione di quelli vecchi in modo efficiente, supportando frequenze LiDAR elevate (>100Hz).
*   **Odometria Diretta:** Registra i punti grezzi direttamente sulla mappa (punti-su-mappa) senza la necessità di estrazione manuale di feature (come bordi o piani), migliorando la robustezza in ambienti poveri di feature.
*   **Fusione Stretta (Tightly-coupled):** Integra i dati LiDAR con l'IMU utilizzando un filtro di Kalman iterativo (IEKF).
*   **Supporto Sensori:** Compatibile con LiDAR rotanti (Hesai, Velodyne, Ouster) e a stato solido (Livox).

### Repository e Risorse
*   **Repository Ufficiale (ROS 1/2):** [hku-mars/FAST_LIO](https://github.com/hku-mars/FAST_LIO)
*   **Branch ROS 2:** [hku-mars/FAST_LIO/tree/ROS2](https://github.com/hku-mars/FAST_LIO/tree/ROS2)
*   **Documentazione Tecnica:** [Fast_LIO_2.pdf](https://github.com/hku-mars/FAST_LIO/blob/main/doc/Fast_LIO_2.pdf) (Articolo scientifico).

### Configurazione per Hesai XT16
Per utilizzare FAST-LIO2 con l'Hesai XT16 sul Go2:
1.  **Formato Dati:** Assicurarsi che il driver del LiDAR pubblichi messaggi di tipo `sensor_msgs/PointCloud2`.
2.  **Parametri IMU:** I dati IMU devono essere sincronizzati. Il Go2 fornisce IMU ad alta frequenza tramite l'SDK.
3.  **Extrinsics:** È necessario calibrare o definire la trasformata statica (estrinseci) tra il LiDAR e l'IMU nel file `.yaml` di configurazione di FAST-LIO2.

## Building and Running

### Prerequisites
*   Ubuntu 22.04 on the robot's onboard computer and the remote laptop.
*   Docker and Docker Compose installed on the robot.
*   ROS 2 Humble installed on the remote laptop.

### Onboard Deployment (Docker)
1.  **Clone dependencies** into `src/`:
    ```bash
    cd src
    git clone https://github.com/unitreerobotics/unitree_ros2.git
    git clone https://github.com/hku-mars/FAST_LIO.git -b ROS2
    ```
2.  **Build and run** the Docker image:
    ```bash
    cd ../docker
    docker compose up --build -d
    ```

### Remote Visualization
On your laptop (connected via Ethernet to the robot):
1.  Set up the ROS 2 environment:
    ```bash
    source /opt/ros/humble/setup.bash
    export ROS_DOMAIN_ID=1  # Match the ID in docker-compose.yml
    ```
2.  Launch Rviz2:
    ```bash
    rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
    ```

## Docker Hub & CI/CD Workflow (Automated)

Questo progetto usa le **GitHub Actions** per compilare e caricare l'immagine Docker automaticamente su Docker Hub ogni volta che carichi il codice su GitHub (branch `main`).

### 1. Come Funziona
Ogni volta che fai `git push origin main`, GitHub:
1.  Prende il tuo codice.
2.  Compila l'immagine Docker.
3.  La carica su Docker Hub con due tag:
    *   `latest`: L'immagine più recente.
    *   `<sha>`: L'identificativo unico del commit (per tornare indietro se serve).

### 2. Come Usare l'Immagine sul Robot
Sul computer di bordo del robot Go2, non è più necessario compilare il codice locale. Ti basta scaricare l'ultima versione:
```bash
cd docker
docker compose pull  # Scarica l'immagine da Docker Hub
docker compose up -d # Avvia il sistema
```

### 3. Configurazione Necessaria
Assicurati che nei **Secrets** della repo GitHub siano presenti:
*   `DOCKERHUB_USERNAME`: Il tuo username Docker Hub.
*   `DOCKERHUB_TOKEN`: Il tuo Personal Access Token di Docker Hub.

## Multi-Machine Development (Desktop vs Laptop)

To work seamlessly across different machines (e.g., Desktop at home and Laptop in the lab), follow this workflow:

### 1. Synchronization via Git
*   **A casa (Desktop):** Esegui `git push` per caricare le modifiche su un repository remoto (es. GitHub).
*   **In Lab (Laptop):** Esegui `git pull` per scaricare l'ultimo stato del progetto.
*   **Coerenza:** Tutte le modifiche fatte "sul campo" in laboratorio devono essere committate e pusshate per ritrovarle sul desktop a casa.

### 2. Coerenza dell'ambiente con Docker
*   Grazie a **Docker**, non è necessario installare ROS 2 o Nav2 localmente su ogni macchina.
*   Il comando `docker compose up --build` garantisce che l'ambiente sia identico sia sul desktop che sul laptop, riducendo gli errori di "funziona solo sulla mia macchina".

### 3. Configurazione Rete (Lab Setup)
*   **Connessione Ethernet:** Collega il laptop al robot Unitree Go2 via cavo Ethernet.
*   **IP Statico Laptop:** Configura la scheda di rete del laptop sullo stesso subnet del robot (es. `192.168.123.10`). L'IP del robot è solitamente `192.168.123.161` o similia.
*   **Docker Host Mode:** Il file `docker-compose.yml` usa `network_mode: host`, permettendo al container di comunicare direttamente con la rete del robot.

## Development Conventions
*   **Bridge Node:** All custom logic for navigation command translation should reside in `src/go2_nav_bridge`.
*   **TF Tree:**
    *   `map` -> `odom` (provided by FAST_LIO2 or SLAM)
    *   `odom` -> `base_link` (provided by FAST_LIO2)
    *   `base_link` -> `lidar_link` (static transform)
*   **Networking:** Ensure the robot's onboard computer and laptop are on the same subnet (default 192.168.123.x for Unitree robots).

## TODO / Roadmap
- [ ] Implement `go2_nav_bridge` node for `cmd_vel` to SDK translation.
- [ ] Configure `FAST_LIO2` parameters for Hesai XT16.
- [ ] Integrate Nav2 parameters for quadrupedal movement.
- [ ] Set up Docker image with all dependencies (SDK, FAST_LIO, Nav2).
