# Base our image from ubuntu bionic
FROM ubuntu:18.04

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
       libpcap-dev
RUN rm -rf /var/lib/apt/lists/*

# Install more recent CMake
ARG CMAKE_VERSION=3.22.0

RUN mkdir ~/cmake-install \
       && cd ~/cmake-install \
       && wget https://github.com/Kitware/CMake/releases/download/v$CMAKE_VERSION/cmake-$CMAKE_VERSION.tar.gz \
       && tar -xzvf cmake-$CMAKE_VERSION.tar.gz \
       && cd cmake-$CMAKE_VERSION/ \
       && ./bootstrap -- -DCMAKE_USE_OPENSSL=OFF \
       && make -j$(nproc) \
       && make install

# Install more recent gcc
ARG GCC_VERSION=8.5.0

RUN export ARCH_NAME=$(uname -m)-$(bash -c "echo \$OSTYPE") \
       && wget https://github.com/gcc-mirror/gcc/archive/refs/tags/releases/gcc-$GCC_VERSION.tar.gz \
       && tar -xvzf gcc-$GCC_VERSION.tar.gz \
       && rm -f gcc-$GCC_VERSION.tar.gz \
       && cd gcc-releases-gcc-$GCC_VERSION \
       && bash ./contrib/download_prerequisites \
       && mkdir _build \
       && cd _build \
       && ../configure -v --build=$ARCH_NAME --host=$ARCH_NAME --target=$ARCH_NAME --prefix=/usr/local/gcc-$GCC_VERSION --enable-checking=release --enable-languages=c,c++ --disable-multilib --program-suffix=-$GCC_VERSION \
       && make -j$(nproc) \
       && make install-strip \
       && cd ../.. \
       && rm -rf gcc-releases-gcc-$GCC_VERSION

ENV PATH=/usr/local/gcc-$GCC_VERSION/bin:$PATH
ENV CC=/usr/local/gcc-$GCC_VERSION/bin/gcc-$GCC_VERSION
ENV CXX=/usr/local/gcc-$GCC_VERSION/bin/g++-$GCC_VERSION

# Install more recent Qt
ARG QT_VERSION=5.15.2

RUN git clone https://code.qt.io/qt/qt5.git \
       && cd qt5 \
       && git checkout $QT_VERSION \
       && perl init-repository --module-subset=essential \
       && mkdir _build \
       && cd _build \
       && ../configure -opensource -nomake examples -nomake tests -confirm-license -skip webengine -skip 3d -skip location -no-opengl \
       && make -j$(nproc) \
       && make install \
       && cd ../.. \
       && rm -rf qt5
# Qt is installed in /usr/local/Qt-$QT_VERSION
# Qt5Config.cmake is installed in /usr/local/Qt-$QT_VERSION/lib/cmake

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
