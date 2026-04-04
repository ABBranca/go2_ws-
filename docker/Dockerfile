FROM ros:humble-ros-base

# Prevent interactive prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# Install essential build and ROS dependencies
RUN apt-get update && apt-get install --no-install-recommends -y \
    build-essential \
    cmake \
    git \
    python3-colcon-common-extensions \
    python3-rosdep \
    ros-humble-rmw-cyclonedds-cpp \
    ros-humble-nav2-bringup \
    ros-humble-slam-toolbox \
    ros-humble-robot-localization \
    ros-humble-teleop-twist-keyboard \
    libpcl-dev \
    ros-humble-pcl-ros \
    && rm -rf /var/lib/apt/lists/*

# Setup rosdep for dependency management
# In this development container, code is mounted via volume, 
# so rosdep will be used to resolve any missing project dependencies.
RUN if [ ! -d /etc/ros/rosdep/sources.list.d ]; then \
      rosdep init; \
    fi && \
    rosdep update

# Set workspace directory
WORKDIR /ros2_ws

# Setup entrypoint for environment configuration
COPY ros_entrypoint.sh /ros_entrypoint.sh
RUN chmod +x /ros_entrypoint.sh

# Persist environment in .bashrc for interactive shells
RUN echo "source /ros_entrypoint.sh" >> ~/.bashrc

ENTRYPOINT ["/ros_entrypoint.sh"]
CMD ["bash"]
