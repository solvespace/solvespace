FROM ubuntu:noble

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    zlib1g-dev \
    libpng-dev \
    libcairo2-dev \
    libfreetype6-dev \
    libjson-c-dev \
    libfontconfig1-dev \
    libgtkmm-4.0-dev \
    libpangomm-2.48-dev \
    libgl-dev \
    libglu-dev \
    libspnav-dev \
    python3 \
    python3-full \
    nodejs \
    npm \
    sudo \
    pkg-config

# Install stylelint for CSS verification using npm
RUN npm install -g stylelint

# Set working directory
WORKDIR /app

# Copy source code
COPY . /app/

# Initialize submodules
RUN git submodule update --init extlib/libdxfrw extlib/mimalloc extlib/eigen

# Build SolveSpace with GTK4
RUN mkdir -p /app/docker-build && \
    cd /app/docker-build && \
    cmake .. \
      -DCMAKE_BUILD_TYPE=Release \
      -DENABLE_OPENMP=ON \
      -DUSE_GTK4=ON && \
    make -j$(nproc)

# Set the entrypoint
ENTRYPOINT ["/bin/bash"]
