# Source this inside the `humble_ws` distrobox before rviz2 / ros2 CLI to talk to the
# robot over WiFi:  source docker/laptop_dds_env.sh
#
# Why each line:
#  - RMW_IMPLEMENTATION: MUST match the robot (CycloneDDS). Default is FastDDS → cross-vendor
#    discovery is unreliable and the laptop sees no topics.
#  - CYCLONEDDS_URI: pins the WiFi interface (wlp3s0) + buffers (see cyclonedds_laptop.xml).
#  - ROS_DOMAIN_ID: matches the robot compose default (0). Both ends must agree.

export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export CYCLONEDDS_URI=file:///home/ale/Documents/Uni/Tesi/go2_ws/docker/cyclonedds_laptop.xml
export ROS_DOMAIN_ID=0

echo "[laptop_dds_env] RMW=$RMW_IMPLEMENTATION DOMAIN=$ROS_DOMAIN_ID URI=$CYCLONEDDS_URI"
