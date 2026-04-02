FROM ros:humble-ros-base

# Install dependencies for Unitree SDK and Nav2
RUN apt-get update && apt-get install -y \
    ros-humble-nav2-bringup \
    ros-humble-slam-toolbox \
    ros-humble-robot-localization \
    ros-humble-teleop-twist-keyboard \
    ros-humble-rmw-cyclonedds-cpp \
    build-essential \
    cmake \
    git \
    python3-colcon-common-extensions \
    libpcl-dev \
    ros-humble-pcl-ros \
    && rm -rf /var/lib/apt/lists/*

# Create workspace directory
WORKDIR /ros2_ws

# Copy the local workspace into the container (optional, usually done via volumes)
# COPY . .

# Set up the environment
SHELL ["/bin/bash", "-c"]
RUN echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc

CMD ["/bin/bash"]
