FROM ros:humble-ros-base

ENV DEBIAN_FRONTEND=noninteractive
SHELL ["/bin/bash", "-c"]

# Install dependencies needed for development
RUN apt-get update && apt-get install -y \
  build-essential \
  cmake \
  git \
  clangd \
  clang-format \
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
  libaprutil1-dev \
  sudo \
  nano \
  gdb \
  curl \
  wget \
  procps \
  libatomic1 \
  lsb-release \
  && rm -rf /var/lib/apt/lists/*

# Add non-root user
ARG USERNAME=ros
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
    && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
    && chmod 0440 /etc/sudoers.d/$USERNAME

USER $USERNAME

# Setup bashrc
RUN echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc \
    && echo "export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp" >> ~/.bashrc \
    && echo "export ROS_DOMAIN_ID=0" >> ~/.bashrc
