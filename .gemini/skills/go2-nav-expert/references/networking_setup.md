# Unitree Go2 Networking & DDS Setup

## Network Topology
- **MCU (Unitree Board)**: `192.168.123.161` (Closed SSH)
- **Expansion Dock (Jetson/PC)**: `192.168.123.18` (Primary ROS 2 Dock)
- **Internal Ethernet**: `192.168.123.0/24`
- **Wi-Fi Interface**: On MCU (Connected to ARSCONTROL)

## DDS Connectivity Strategy
To bridge ROS 2 from internal Eth to Wi-Fi:
1.  **DDS Discovery Server**: Start a server on the Dock (`.18`) and point laptop's RMW to it.
2.  **Initial Peers**: Configure `CYCLONEDDS_URI` or `RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` with specific peers if multicasting is blocked.

## CycloneDDS XML Configuration (example.xml)
```xml
<CycloneDDS>
  <Domain>
    <General>
      <NetworkInterfaceAddress>eth0</NetworkInterfaceAddress>
      <AllowMulticast>false</AllowMulticast>
    </General>
    <Discovery>
      <Peers>
        <Peer Address="192.168.123.18"/>
      </Peers>
    </Discovery>
  </Domain>
</CycloneDDS>
```

## Connectivity Checks
- `ping 192.168.123.18` from laptop (connected via RJ45).
- `ros2 node list` inside Docker to verify intra-container discovery.
- `ros2 topic list` from laptop to verify bridge to host.
