# Unitree Go2 - Autonomous Navigation Setup

Benvenuto nel repository per la navigazione autonoma del robot Unitree Go2. Questa guida spiega come configurare l'ambiente, sviluppare il codice e distribuirlo sul robot.

---

## 1. Preparazione Ambiente (Una tantum)

### Hardware Necessario
*   **Robot**: Unitree Go2.
*   **LiDAR**: Hesai XT16 (collegato all'Expansion Dock).
*   **PC/Laptop**: Ubuntu 22.04 (raccomandato) con porta Ethernet.
*   **Cavo Ethernet**: Per il collegamento PC-Robot.

### Configurazione Rete Laptop
Collega il cavo Ethernet all'**Expansion Dock** del robot e configura la connessione cablata del tuo laptop:
*   **IPv4 Manuale**: `192.168.123.10`
*   **Netmask**: `255.255.255.0`
*   **Gateway**: `192.168.123.1` (Opzionale)

### Indirizzi IP Critici del Robot
*   `192.168.123.161`: Motion Control Unit (MCU). Gestisce i motori. **SSH bloccato**.
*   `192.168.123.18`: Expansion Dock (Orin). Qui risiede lo stack di navigazione. **Accesso SSH abilitato**.

---

## 2. Download del Progetto

Clona il repository con tutti i sottomoduli necessari:
```bash
git clone --recursive https://github.com/ABBranca/go2_ws.git
cd go2_ws
```
*Se hai già clonato senza sottomoduli, esegui:*
```bash
git submodule update --init --recursive
```

---

## 3. Workflow di Sviluppo Rapido (Prototipazione)

Per velocizzare lo sviluppo ed evitare di ricostruire l'immagine Docker a ogni modifica del codice:

### A. Sincronizzazione Codice (Laptop -> Robot)
Modifica il codice localmente sul tuo laptop (in `src/`), poi invia i cambiamenti al robot via Ethernet:
```bash
chmod +x sync_to_dog.sh
./sync_to_dog.sh
```

### B. Compilazione ed Esecuzione (sul Robot)
Accedi al container Docker sul robot per compilare ed eseguire:
```bash
# Entra nel container
docker exec -it go2_navigation bash

# Compila (grazie a --symlink-install, le modifiche YAML/Python sono istantanee)
colcon build --symlink-install
source install/setup.bash

# Esempio: lancia il bridge di navigazione
ros2 launch go2_nav_bridge mapping.launch.py
```

### C. Visualizzazione Remota (sul Laptop)
Apri Rviz2 sul tuo laptop per monitorare il robot:
```bash
source /opt/ros/humble/setup.bash
export ROS_DOMAIN_ID=1
rviz2 -d src/go2_nav_bridge/rviz/nav2.rviz
```

---

## 4. Deployment Finale (Immagine Immutabile)

Quando lo sviluppo è completo e vuoi creare un'immagine definitiva ("Piano B"):

### A. Build dell'immagine ARM64 (sul Laptop)
```bash
docker buildx build --platform linux/arm64 -t go2_nav_stack:latest --load .
```

### B. Trasferimento e Caricamento (sul Robot)
Invia l'immagine compressa direttamente al Docker del robot:
```bash
docker save go2_nav_stack:latest | ssh -C unitree@192.168.123.18 'docker load'
```

---

## Struttura del Workspace (Submoduli)
*   **unitree_ros2**: SDK ufficiale per la comunicazione con l'hardware Unitree.
*   **hesai_ros_driver_2**: Driver per il LiDAR Hesai XT16.
*   **fast_lio_ros2**: Framework di odometria LiDAR-Inerziale (SLAM).
*   **go2_nav_bridge**: (Package Locale) Traduce i comandi `cmd_vel` di Nav2 in comandi Unitree SDK.

---
**Contatti**: Per domande tecniche o supporto, contatta [ABBranca](https://github.com/ABBranca).
