# Unitree Go2 - Autonomous Navigation Setup

Welcome to the repository for the autonomous navigation of the Unitree Go2 robot. This guide is designed to be simple and clear, even if you don't have much experience with programming or Linux.

## What is this project?
The goal is to enable the Go2 robot to move autonomously using:
*   **ROS 2 Humble**: The "brain" that manages sensors and motors.
*   **LiDAR Hesai XT16**: The laser sensor that allows the robot to "see".
*   **Docker**: A tool that creates a "closed box" with all the software already installed, so you don't have to configure anything on your computer.

---

## Preparation (One-time only)

### 1. Hardware Requirements
*   **Robot**: Unitree Go2.
*   **PC/Laptop**: Ubuntu 22.04 installed (recommended) and an Ethernet port.
*   **Ethernet Cable**: To connect the PC to the robot.

### 2. Installing Docker
If you don't have Docker, install it with these simple commands in your terminal:
```bash
sudo apt update
sudo apt install docker.io docker-compose -y
sudo usermod -aG docker $USER
```
*Restart your computer after these commands.*

---

## How to Start the System

### Step 1: Connecting to the Robot
1.  Connect the Ethernet cable between your laptop and the Go2 robot.
2.  Configure your laptop's network:
    *   Go to Ubuntu's network settings.
    *   Select the wired connection (Ethernet) and set **IPv4 to "Manual"**.
    *   **Address**: `192.168.123.10`
    *   **Netmask**: `255.255.255.0`
    *   **Gateway**: `192.168.123.1`
3.  Verify the connection by typing in the terminal: `ping 192.168.123.161` (you should see response times).

### Step 2: Download the Project
Open the terminal and type:
```bash
git clone https://github.com/ABBranca/go2_ws.git
cd go2_ws
```

### Step 3: Start the Environment (Docker)
Enter the `docker` folder and start everything:
```bash
cd docker
docker compose up --build -d
```
*This command will automatically download and configure everything needed. It may take a few minutes the first time.*

---

## Visualization (Rviz2)
To see what the robot "sees" from your laptop:
1.  Open a new terminal.
2.  Type:
    ```bash
    rviz2
    ```
3.  Load the configuration found in `src/go2_nav_bridge/rviz/nav2.rviz` (File -> Open Config).

---

## Common Troubleshooting
*   **The robot does not respond**: Check the cables and ensure the laptop's IP is `192.168.123.10`.
*   **Docker gives permission errors**: Make sure you ran the `usermod` command and restarted.
*   **Commands not found**: Ensure you are inside the project folder (`go2_ws`).

---

**Contacts**: For technical questions or support, contact [ABBranca](https://github.com/ABBranca).
