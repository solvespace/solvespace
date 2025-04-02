FROM ubuntu:24.04

# Install required packages
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libgtkmm-4.0-dev \
    libgtk-4-dev \
    libjson-c-dev \
    libfontconfig-dev \
    libspnav-dev \
    libgl1-mesa-dev \
    git \
    zlib1g-dev \
    libpng-dev \
    libfreetype6-dev \
    libcairo2-dev \
    libpangomm-2.48-dev \
    libx11-dev \
    gettext \
    pkg-config \
    libgdk-x11-4.0-dev

# Set up working directory
WORKDIR /app

# Clone the repository (optional, can also be mounted)
# RUN git clone https://github.com/solvespace/solvespace.git

# Set working directory
WORKDIR /app/solvespace

# Command to run when container starts
CMD ["/bin/bash"]
