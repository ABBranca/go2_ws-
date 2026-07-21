#!/bin/bash
set -e

# Setup ROS 2 environment
# shellcheck source=/dev/null
source "/opt/ros/$ROS_DISTRO/setup.bash"

# Setup local workspace if built
if [ -f "/ros2_ws/install/setup.bash" ]; then
  # shellcheck source=/dev/null
  source "/ros2_ws/install/setup.bash"
fi

# ── Pre-flight: kernel socket buffer for CycloneDDS ───────────────────────────
# cyclonedds.xml requests <SocketReceiveBufferSize min="4 MiB"/>. If the host
# kernel's net.core.rmem_max is below 4 MiB, setsockopt(SO_RCVBUF) is clamped
# and CycloneDDS aborts DomainParticipant creation → rmw_create_node fails →
# downstream nodes crash with SIGABRT (exit -6). Catch this early with a clear
# operator-actionable error instead of an opaque C++ stack trace at runtime.
#
# Override with SKIP_DDS_PREFLIGHT=1 (e.g. for dev shells where the stack
# is not launched).
REQUIRED_BUF=4194304   # 4 MiB
if [ "${SKIP_DDS_PREFLIGHT:-0}" != "1" ]; then
  RMEM_MAX=$(cat /proc/sys/net/core/rmem_max 2>/dev/null || echo 0)
  WMEM_MAX=$(cat /proc/sys/net/core/wmem_max 2>/dev/null || echo 0)
  if [ "$RMEM_MAX" -lt "$REQUIRED_BUF" ] || [ "$WMEM_MAX" -lt "$REQUIRED_BUF" ]; then
    echo "════════════════════════════════════════════════════════════════════" >&2
    echo "FATAL: host kernel socket buffers too small for CycloneDDS"           >&2
    echo "  net.core.rmem_max = $RMEM_MAX   (required ≥ $REQUIRED_BUF)"         >&2
    echo "  net.core.wmem_max = $WMEM_MAX   (required ≥ $REQUIRED_BUF)"         >&2
    echo                                                                        >&2
    echo "Fix on the HOST (Orin Dock), not inside the container:"               >&2
    echo "  sudo tee /etc/sysctl.d/99-cyclonedds.conf <<'EOF'"                  >&2
    echo "  net.core.rmem_max=$REQUIRED_BUF"                                    >&2
    echo "  net.core.wmem_max=$REQUIRED_BUF"                                    >&2
    echo "  EOF"                                                                >&2
    echo "  sudo sysctl --system"                                               >&2
    echo                                                                        >&2
    echo "To bypass this check (dev shells only), set SKIP_DDS_PREFLIGHT=1"     >&2
    echo "════════════════════════════════════════════════════════════════════" >&2
    exit 1
  fi
fi

exec "$@"
