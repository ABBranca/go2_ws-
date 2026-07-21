# WIRED path. Source inside the `humble_ws` distrobox before rviz2 / ros2 CLI:
#   source docker/laptop_dds_env_wired.sh
# Requires the Ethernet cable to the Orin Dock plugged in (eno1 UP, 192.168.123.222).

export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export CYCLONEDDS_URI=file:///home/ale/Documents/Uni/Tesi/go2_ws/docker/cyclonedds_laptop_wired.xml
export ROS_DOMAIN_ID=0

echo "[laptop_dds_env_wired] RMW=$RMW_IMPLEMENTATION DOMAIN=$ROS_DOMAIN_ID URI=$CYCLONEDDS_URI"
