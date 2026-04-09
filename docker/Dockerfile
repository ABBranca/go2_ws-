# =============================================================================
# Stage 1 — builder
# Compiles the full ROS 2 workspace from source.
# The resulting install/ tree is copied into the leaner runtime stage.
# =============================================================================
FROM ros:humble-ros-base AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install --no-install-recommends -y \
    build-essential \
    cmake \
    git \
    python3-colcon-common-extensions \
    python3-rosdep \
    ros-humble-rmw-cyclonedds-cpp \
    ros-humble-nav2-bringup \
    ros-humble-nav2-mppi-controller \
    ros-humble-nav2-smac-planner \
    ros-humble-robot-localization \
    libpcl-dev \
    ros-humble-pcl-ros \
    && rm -rf /var/lib/apt/lists/*

# Initialise rosdep (skip if already done in the base image)
RUN if [ ! -f /etc/ros/rosdep/sources.list.d/20-default.list ]; then \
      rosdep init; \
    fi && \
    rosdep update

WORKDIR /ros2_ws

# Build context is the project root (set in docker-compose.yml via context: ..)
COPY src/ /ros2_ws/src/

# Resolve any remaining apt-installable deps declared in package.xml files.
# unitree_go / unitree_api are built from source — skip their rosdep keys.
RUN . /opt/ros/humble/setup.sh && \
    rosdep install --from-paths src --ignore-src -r -y \
        --skip-keys "unitree_go unitree_api"

# Build the workspace
RUN . /opt/ros/humble/setup.sh && \
    colcon build \
        --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble \
        --no-warn-unused-cli

# =============================================================================
# Stage 2 — runtime
# Minimal image: only runtime libraries + the pre-compiled install/ tree.
# No build toolchain, no rosdep, no colcon.
# =============================================================================
FROM ros:humble-ros-base AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install --no-install-recommends -y \
    ros-humble-rmw-cyclonedds-cpp \
    ros-humble-nav2-bringup \
    ros-humble-nav2-mppi-controller \
    ros-humble-nav2-smac-planner \
    ros-humble-robot-localization \
    ros-humble-pcl-ros \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ros2_ws

# Copy only the compiled artefacts from the builder stage
COPY --from=builder /ros2_ws/install /ros2_ws/install

# Entrypoint: sources ROS 2 + workspace, then exec CMD
COPY docker/ros_entrypoint.sh /ros_entrypoint.sh
RUN chmod +x /ros_entrypoint.sh

# Persist sourcing for interactive shells
RUN echo "source /ros_entrypoint.sh" >> ~/.bashrc

ENTRYPOINT ["/ros_entrypoint.sh"]
CMD ["ros2", "launch", "go2_nav_bridge", "bringup.launch.py"]
