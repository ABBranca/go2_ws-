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
The system consists of two primary onboard computing units connected via an internal Ethernet bridge:
1.  **Integrated Computer (MCU) - `192.168.123.161`**: 
    *   Handles motion control and low-level SDK functions.
    *   **Connectivity**: Equipped with a Wireless adapter (connected to laboratory Wi-Fi).
    *   **Access**: SSH is currently **refused** on this unit (closed system).
2.  **Expansion Dock (Orin/PC) - `192.168.123.18`**: 
    *   Runs the high-level navigation stack (ROS 2 Humble in Docker).
    *   **Connectivity**: No native Wi-Fi. Connected to the MCU via internal Ethernet and to the developer laptop via external RJ45.
    *   **Role**: Processes FAST_LIO2, Nav2, and the `go2_nav_bridge`.

## Milestone Status (March 27, 2026)
- [x] **Wi-Fi Connected**: MCU successfully connected to "ARSCONTROL" via the Unitree Go app.
- [x] **Submodules Initialized**: All submodules are fully updated.
- [x] **Dock Access**: Established SSH access to the Expansion Dock (`192.168.123.18`).
- [x] **Networking Diagnosis**: Identified that the MCU does not automatically bridge ROS 2 traffic from the internal Ethernet to the Wi-Fi interface.
- [ ] **Wireless Telemetry**: Resolve the "Connection Refused" on MCU or configure a DDS Discovery Server/Static Peers to route Rviz2 data through the MCU's Wi-Fi.

## Networking Topology & Challenges
The primary challenge is routing ROS 2 messages from the **Dock** (where the SLAM/Nav nodes live) to a **Remote Laptop** via the **MCU's Wi-Fi** adapter. 

*   **Internal Network**: `192.168.123.0/24`.
*   **External Network**: Wi-Fi LAN (e.g., `10.0.0.0/24`).
*   **The Blocker**: Since SSH is disabled on the MCU (`.161`), we cannot easily enable IP Forwarding or NAT rules on the robot's gateway. We must rely on ROS 2 DDS configurations (Discovery Server or Initial Peers) to traverse the network.

## Proposed Strategic Decisions
### 1. Dedicated Wi-Fi Dongle for Expansion Dock (Preferred)
Dato che l'accesso SSH all'MCU (`.161`) è precluso, la configurazione di un bridge o di regole di routing/NAT a livello kernel non è attuabile. 
*   **Proposta**: Installare un dongle Wi-Fi USB direttamente sull'Expansion Dock (Orin/PC).
*   **Vantaggi**: 
    *   Il Dock otterrà un proprio indirizzo IP sulla rete di laboratorio ("ARSCONTROL").
    *   Il laptop e il Dock saranno sulla stessa sottorete L2/L3, eliminando la necessità di attraversare l'MCU per la telemetria ROS 2.
    *   **Semplificazione immediata della comunicazione DDS senza necessità di configurazioni complesse (Discovery Server).
    *   **Hardware Consigliato**: **Alfa Network AWUS036ACM** (chipset Mediatek MT7612U) o modelli con supporto Linux nativo (evitare chipset che richiedono compilazione driver manuale su Jetson/ARM).
    *   **Stato**: Proposta da discutere con i supervisori per approvazione e acquisto hardware. (Pending meeting con i superiori).

## Development Workflow (Current Strategy)
Per massimizzare la velocità di prototipazione in attesa dell'hardware Wi-Fi:
1.  **Physical Link**: Robot collegato al laptop via cavo Ethernet (RJ45 su Expansion Dock).
2.  **Code Sync**: Sviluppo locale su laptop -> Sincronizzazione via `rsync` o `scp` verso `/home/unitree/go2_ws/src` sul Dock.
3.  **Docker Dev Mode**: Utilizzo di un `docker-compose.yml` provvisorio con **mount dei volumi** per riflettere istantaneamente le modifiche al codice senza rebuild dell'immagine.
    *   *Nota*: Il volume risiede fisicamente sul Dock e viene mappato nel container.
4.  **Remote Visualization**: Rviz2 eseguito su laptop puntando all'IP del Dock (`192.168.123.18`).

## Upcoming Tasks & Thesis Analysis
Verrà fornita una tesi di un precedente studente sul binomio Unitree Go2 + LiDAR. Obiettivi dell'analisi:
*   **Networking**: Identificare come è stata risolta la connettività Wi-Fi e la telemetria remota (DDS, Bridge, o hardware dedicato).
*   **Calibration**: Estrarre le matrici estrinseche (TF) tra il corpo del Go2 e il LiDAR Hesai.
*   **FAST_LIO2 Tuning**: Recuperare i parametri di rumore (noise) dell'IMU e configurazioni specifiche per il sensore XT16.

## External Resources & Community Solutions
### 1. Key GitHub Repositories
*   **[abizovnuralem/go2_ros2_sdk](https://github.com/abizovnuralem/go2_ros2_sdk)**: Supporta **WebRTC** per telemetria fluida via Wi-Fi. Utile se il DDS standard risulta troppo pesante/instabile.
*   **[unitree_go2_nav](https://github.com/Sayantani-Bhattacharya/unitree_go2_nav)**: Esempio specifico di navigazione autonoma con Nav2 e SLAM RTAB-Map su Go2.
*   **[Unitree-Go2-Robot/go2_robot](https://github.com/Unitree-Go2-Robot/go2_robot)**: Integrazione standard per `cmd_vel` e URDF completo.

### 2. Networking Strategies Identified
*   **WebRTC/DDS Hybrid**: L'uso di WebRTC per i flussi video/Lidar e DDS per i comandi di controllo sembra essere il gold standard per il wireless.
*   **IP Forwarding on Jetson**: Se si usa un dongle Wi-Fi, è possibile trasformare il Jetson (`.18`) in un gateway per raggiungere l'MCU (`.161`):
    ```bash
    sudo sysctl -w net.ipv4.ip_forward=1
    sudo iptables -t nat -A POSTROUTING -o eth0 -j MASQUERADE
    ```
*   **Travel Router Option**: Un router esterno (es. GL.iNet) collegato alla porta Ethernet del Go2 per creare un bridge L2 trasparente (soluzione hardware alternativa al dongle).

---

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

## Building and Running

### Prerequisites
*   Ubuntu 22.04 on the robot's onboard computer and the remote laptop.
*   Docker and Docker Compose installed on the robot.
*   ROS 2 Humble installed on the remote laptop.

### Onboard Deployment (Docker)
1.  **Initialize Submodules** (if not already done):
    ```bash
    git submodule update --init --recursive
    ```
2.  **Build and run** the Docker image:
    ```bash
    cd docker
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

## Development Conventions
*   **Bridge Node:** All custom logic for navigation command translation should reside in `src/go2_nav_bridge`.
*   **Message Types**: Prefer high-level `SportModeCmd` for movement to leverage the robot's onboard stability controllers.
*   **TF Tree:**
    *   `map` -> `odom` (provided by FAST_LIO2)
    *   `odom` -> `base_link` (provided by FAST_LIO2)
    *   `base_link` -> `lidar_link` (static transform)

## TODO / Roadmap
- [x] Integrate ROS drivers for Hesai XT16 LiDAR.
- [x] Connect the robot to the laboratory Wi-Fi (ARSCONTROL).
- [x] Initialize and update all Git submodules.
- [ ] Implement `go2_nav_bridge` node for `cmd_vel` to `SportModeCmd` translation.
- [ ] Build and deploy the ROS 2 Humble Docker stack using `docker save/load` (Piano B).
- [ ] Configure `FAST_LIO2` parameters for Hesai XT16 (noise, extrinsics).
- [ ] Integrate Nav2 parameters for quadrupedal movement.
