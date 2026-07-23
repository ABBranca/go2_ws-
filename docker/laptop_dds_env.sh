# Backward-compat shim. The canonical laptop DDS/ROS environment now lives in
# scripts/setup_laptop_env.sh, which sources ROS + the local overlay and points
# CYCLONEDDS_URI at the single transport-agnostic cyclonedds_laptop.xml (autodetermine
# → works both wired and over the Wi-Fi AP). Prefer sourcing that directly:
#     source scripts/setup_laptop_env.sh
_here="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
source "${_here}/../scripts/setup_laptop_env.sh"
unset _here
