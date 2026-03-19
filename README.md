# Unitree Go2 - Autonomous Navigation Setup

Benvenuto nel repository per la navigazione autonoma del robot Unitree Go2. Questa guida è pensata per essere semplice e chiara, anche se non hai molta esperienza con la programmazione o Linux.

## Cos'è questo progetto?
L'obiettivo è far muovere il robot Go2 in modo autonomo usando:
*   **ROS 2 Humble**: Il "cervello" che gestisce i sensori e i motori.
*   **LiDAR Hesai XT16**: Il sensore laser che permette al robot di "vedere".
*   **Docker**: Uno strumento che crea una "scatola chiusa" con tutto il software già installato, così non devi configurare nulla sul tuo computer.

---

## Preparazione (Da fare una sola volta)

### 1. Requisiti Hardware
*   **Robot**: Unitree Go2.
*   **PC/Laptop**: Ubuntu 22.04 installato (consigliato) e una porta Ethernet.
*   **Cavo Ethernet**: Per collegare il PC al robot.

### 2. Installazione di Docker
Se non hai Docker, installalo con questi semplici comandi sul tuo terminale:
```bash
sudo apt update
sudo apt install docker.io docker-compose -y
sudo usermod -aG docker $USER
```
*Riavvia il computer dopo questi comandi.*

---

## Come Avviare il Sistema

### Passaggio 1: Collegamento al Robot
1.  Collega il cavo Ethernet tra il tuo laptop e il robot Go2.
2.  Configura la rete del tuo laptop:
    *   Vai nelle impostazioni di rete di Ubuntu.
    *   Seleziona la connessione cablata (Ethernet) e imposta l'**IPv4 su "Manual"**.
    *   **Address**: `192.168.123.10`
    *   **Netmask**: `255.255.255.0`
    *   **Gateway**: `192.168.123.1`
3.  Verifica la connessione scrivendo nel terminale: `ping 192.168.123.161` (dovresti vedere dei tempi di risposta).

### Passaggio 2: Scaricare il Progetto
Apri il terminale e scrivi:
```bash
git clone https://github.com/ABBranca/go2_ws.git
cd go2_ws
```

### Passaggio 3: Avvio dell'ambiente (Docker)
Entra nella cartella `docker` e avvia tutto:
```bash
cd docker
docker compose up --build -d
```
*Questo comando scaricherà e configurerà tutto il necessario automaticamente. Potrebbe metterci qualche minuto la prima volta.*

---

## Visualizzazione (Rviz2)
Per vedere cosa "vede" il robot dal tuo laptop:
1.  Apri un nuovo terminale.
2.  Digita:
    ```bash
    rviz2
    ```
3.  Carica la configurazione che trovi in `src/go2_nav_bridge/rviz/nav2.rviz` (File -> Open Config).

---

## Risoluzione Problemi Comuni
*   **Il robot non risponde**: Controlla i cavi e che l'IP del laptop sia `192.168.123.10`.
*   **Docker dà errore di permessi**: Assicurati di aver eseguito il comando `usermod` e di aver riavviato.
*   **Comandi non trovati**: Assicurati di essere dentro la cartella del progetto (`go2_ws`).

---

**Contatti**: Per domande tecniche o supporto, contatta [ABBranca](https://github.com/ABBranca).
