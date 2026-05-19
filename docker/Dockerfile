# syntax=docker/dockerfile:1
# =============================================================================
# Stage 1 — builder
# =============================================================================
FROM ros:humble-ros-base AS builder

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]

# 1. Install Build Tools and Extra Dependencies
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update && apt-get install -y \
  build-essential \
  cmake \
  curl \
  git \
  python3-colcon-common-extensions \
  python3-rosdep \
  python3-numpy \
  ros-humble-rmw-cyclonedds-cpp \
  ros-humble-nav2-bringup \
  ros-humble-nav2-mppi-controller \
  ros-humble-nav2-smac-planner \
  ros-humble-robot-localization \
  ros-humble-octomap-server \
  ros-humble-octomap-ros \
  ros-humble-rosbag2-storage-mcap \
  ros-humble-diagnostic-updater \
  libpcl-dev \
  ros-humble-pcl-ros \
  libapr1-dev \
  libaprutil1-dev

# GLIM GPU-accelerated SLAM (headers for colcon build; CUDA provided by NVIDIA runtime)
RUN curl -fsSL https://koide3.github.io/ppa/setup_ppa.sh | bash && \
    apt-get update && apt-get install -y \
      ros-humble-glim-ros \
      libgtsam-points-dev \
    && rm -rf /var/lib/apt/lists/*

# 2. Update rosdep (already initialized in base image)
RUN rosdep update

WORKDIR /ros2_ws

# Optimization: Copy source
COPY src/ /ros2_ws/src/

# Extra cleanup of any local build artifacts that might have escaped .dockerignore
RUN find src/ -type d -name "build" -exec rm -rf {} + && \
    find src/ -type d -name "install" -exec rm -rf {} + && \
    find src/ -type d -name "log" -exec rm -rf {} +

# Exclude example packages that require the native unitree_sdk2 (not available via apt)
RUN touch src/unitree_ros2/example/COLCON_IGNORE

RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt/lists,sharing=locked \
    apt-get update && . /opt/ros/humble/setup.sh && \
  rosdep install --from-paths src --ignore-src -r -y \
  --skip-keys "unitree_go unitree_api"

# 3. Build the workspace
RUN --mount=type=cache,target=/ros2_ws/build,sharing=private \
    --mount=type=cache,target=/ros2_ws/log,sharing=private \
    . /opt/ros/humble/setup.sh && \
    export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp && \
    colcon build --parallel-workers 1 \
      --packages-select unitree_api unitree_go \
      --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble && \
    colcon build --parallel-workers 1 \
      --cmake-args -DROS_EDITION=ROS2 -DHUMBLE_ROS=humble

# =============================================================================
# Stage 2 — runtime
# =============================================================================
FROM ros:humble-ros-base AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
  ros-humble-rmw-cyclonedds-cpp \
  ros-humble-nav2-bringup \
  ros-humble-nav2-mppi-controller \
  ros-humble-nav2-smac-planner \
  ros-humble-robot-localization \
  ros-humble-octomap-server \
  ros-humble-octomap-ros \
  ros-humble-pcl-ros \
  libapr1 \
  libaprutil1 \
  && rm -rf /var/lib/apt/lists/*

# GLIM GPU-accelerated SLAM (shared libs for runtime; CUDA provided by NVIDIA runtime)
RUN curl -fsSL https://koide3.github.io/ppa/setup_ppa.sh | bash && \
    apt-get update && apt-get install -y \
      ros-humble-glim-ros \
      libgtsam-points-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /ros2_ws
COPY --from=builder /ros2_ws/install /ros2_ws/install

COPY docker/ros_entrypoint.sh /ros_entrypoint.sh
RUN chmod +x /ros_entrypoint.sh
RUN echo "source /ros_entrypoint.sh" >> ~/.bashrc

ENTRYPOINT ["/ros_entrypoint.sh"]
CMD ["ros2", "launch", "go2_nav_bridge", "bringup.launch.py"]
