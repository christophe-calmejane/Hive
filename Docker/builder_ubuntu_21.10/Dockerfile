# Base our image from ubuntu impish
FROM ubuntu:21.10

# Install required packages
RUN apt-get update
RUN DEBIAN_FRONTEND=noninteractive apt-get -qq -y install \
       wget \
       vim \
       sudo \
       git \
       build-essential \
       ninja-build \
       g++ \
       flex \
       python \
       ncurses-dev \
       libpcap-dev \
       qtbase5-dev
RUN rm -rf /var/lib/apt/lists/*

# Install more recent CMake
ARG CMAKE_VERSION=3.22.5

RUN mkdir ~/cmake-install \
       && cd ~/cmake-install \
       && wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION.tar.gz \
       && tar -xzvf cmake-$CMAKE_VERSION.tar.gz \
       && cd cmake-$CMAKE_VERSION/ \
       && ./bootstrap -- -DCMAKE_USE_OPENSSL=OFF \
       && make -j$(nproc) \
       && make install

# Create a user
ENV USERNAME=builder
ENV PASSWORD=builder

RUN useradd -rm -d /home/$USERNAME -s /bin/bash -g root -G sudo -u 1000 $USERNAME
RUN echo "$PASSWORD\n$PASSWORD" | passwd $USERNAME
RUN echo '%sudo ALL=(ALL) NOPASSWD:ALL' >> /etc/sudoers

# Switch to user
WORKDIR /home/$USERNAME
USER $USERNAME

# Create a folder to mount sources
RUN mkdir -p ~/sources

# Create a folder to mount build tree
RUN mkdir -p ~/builds

# Start in sources
WORKDIR /home/$USERNAME/sources

# Default command
CMD [ "bash" ]
