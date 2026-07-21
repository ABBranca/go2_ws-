# --- tree-sitter CLI builder (glibc-compatible) ---
# nvim-treesitter `main` branch (used by LazyVim, pinned in dotfiles) no longer compiles
# parser.c with `cc` directly: it shells out to the `tree-sitter` CLI (>=0.26.1) for
# `tree-sitter generate` / `tree-sitter build`. Every prebuilt CLI (npm AND GitHub release)
# for the 0.26.x series is linked against glibc 2.39, but this base is Ubuntu 22.04 /
# glibc 2.35 — so those binaries fail with `GLIBC_2.39 not found` and all parser builds die.
# Fix: compile the CLI from source on Debian bullseye (glibc 2.31); the resulting binary
# runs fine on 2.35. Only the ~5 MB binary is copied into the final image (no Rust bloat).
FROM rust:1-bullseye AS ts-builder
# tree-sitter-cli pulls rquickjs-sys (the JS runtime for `tree-sitter generate`), whose
# build.rs runs bindgen — which needs libclang at build time. Builder-only: these packages
# never reach the final image. Bullseye glibc is 2.31, so the binary needs only <=GLIBC_2.30
# and runs on the 2.35 base.
RUN apt-get update && apt-get install -y --no-install-recommends libclang-dev clang \
  && rm -rf /var/lib/apt/lists/*
RUN cargo install tree-sitter-cli --version '^0.26.1' --root /out

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
  ros-humble-pointcloud-to-laserscan \
  libapr1-dev \
  libaprutil1-dev \
  sudo \
  nano \
  gdb \
  curl \
  neovim \
  wget \
  procps \
  libatomic1 \
  lsb-release \
  ripgrep \
  fd-find \
  unzip \
  jq \
  python3-venv \
  python3-pip \
  wl-clipboard \
  xclip \
  && rm -rf /var/lib/apt/lists/*

# --- Node.js 20 (LazyVim/Mason LSP toolchain) ---
# Ubuntu 22.04 ships Node 12 via apt (too old for pyright & most language servers).
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
  && apt-get install -y --no-install-recommends nodejs \
  && rm -rf /var/lib/apt/lists/*

# --- Neovim (recent release; Ubuntu 22.04 apt ships 0.6, too old for LazyVim >= 0.10) ---
# NB: verificare l'asset name del release: dalla 0.10.4 e' `nvim-linux-x86_64.tar.gz`
#     (prima era `nvim-linux64.tar.gz`).
ARG NVIM_VERSION=stable
RUN curl -fsSL -o /tmp/nvim.tar.gz \
  "https://github.com/neovim/neovim/releases/download/${NVIM_VERSION}/nvim-linux-x86_64.tar.gz" \
  && tar -xzf /tmp/nvim.tar.gz -C /opt \
  && ln -s /opt/nvim-linux-x86_64/bin/nvim /usr/local/bin/nvim \
  && rm /tmp/nvim.tar.gz

# --- tree-sitter CLI (for nvim-treesitter `main`; built in ts-builder stage, see top of file) ---
# /usr/local/bin is on PATH for all users, so nvim-treesitter's `tree-sitter build` resolves it.
COPY --from=ts-builder /out/bin/tree-sitter /usr/local/bin/tree-sitter

# Add non-root user
ARG USERNAME=ros
ARG USER_UID=1000
ARG USER_GID=$USER_UID

RUN groupadd --gid $USER_GID $USERNAME \
  && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME \
  && echo $USERNAME ALL=\(root\) NOPASSWD:ALL > /etc/sudoers.d/$USERNAME \
  && chmod 0440 /etc/sudoers.d/$USERNAME

# Pre-create colcon artifact dirs so the named volumes (see devcontainer.json)
# initialize with ros ownership instead of root. Same principle as the nvim dirs below.
RUN mkdir -p /workspaces/go2_ws/build /workspaces/go2_ws/install /workspaces/go2_ws/log \
  && chown -R $USER_UID:$USER_GID /workspaces

USER $USERNAME

# Setup bashrc
RUN echo "source /opt/ros/humble/setup.bash" >> ~/.bashrc \
  && echo "export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp" >> ~/.bashrc \
  && echo "export ROS_DOMAIN_ID=0" >> ~/.bashrc \
  && echo "[ -f /workspaces/go2_ws/install/setup.bash ] && source /workspaces/go2_ws/install/setup.bash" >> ~/.bashrc

# Pre-create Neovim data dirs owned by 'ros' so the named volumes mounted here
# (see devcontainer.json "mounts") inherit ros:ros ownership instead of root.
RUN mkdir -p ~/.local/share/nvim ~/.local/state/nvim ~/.cache/nvim ~/.config
