#!/bin/bash

set -e
set -o xtrace

# Add ARM64 architecture
dpkg --add-architecture arm64

# Add bullseye-backports for newer packages
echo 'deb [trusted=yes] http://archive.debian.org/debian bullseye main contrib non-free' | tee -a /etc/apt/sources.list
echo 'deb [trusted=yes] http://archive.debian.org/debian bullseye-updates main contrib non-free' | tee -a /etc/apt/sources.list
echo 'deb [trusted=yes] http://archive.debian.org/debian bullseye-backports main contrib non-free' | tee -a /etc/apt/sources.list

apt -o Acquire::Check-Valid-Until=false update
apt purge -y libjpeg-dev libjpeg-turbo8-dev libssl-dev
apt update

# Set PKG_CONFIG_PATH for ARM64
export PKG_CONFIG_PATH=/usr/lib/aarch64-linux-gnu/pkgconfig

# Install ARM64 cross toolchain and headers
apt-get install -y --allow-downgrades \
  crossbuild-essential-arm64 \
  gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
  meson/bullseye-backports \
  pkg-config \
  python3-attr python3-jinja2 python3-mako python3-numpy python3-six \
  libx11-dev:arm64 libxext-dev:arm64 libxau-dev:arm64 libxdmcp-dev:arm64 \
  libxcb1-dev:arm64 libxcb-shm0-dev:arm64 libxcb-xkb-dev:arm64 \
  libxfont-dev:arm64 libpixman-1-dev:arm64 libpciaccess-dev:arm64 \
  libdrm-dev:arm64 libinput-dev:arm64 libevdev2:arm64 \
  libxkbfile-dev:arm64 libxkbcommon-dev:arm64 \
  libegl1-mesa-dev:arm64 libgbm-dev:arm64 libepoxy-dev:arm64 \
  libpango1.0-dev:arm64 libudev-dev:arm64 libsystemd-dev:arm64 \
  libunwind-dev:arm64 libselinux1-dev:arm64 libffi-dev:arm64 \
  libbsd-dev:arm64 libexpat-dev:arm64 libcairo2-dev:arm64 \
  libxrandr-dev:arm64 libxrender-dev:arm64 libxfixes-dev:arm64 \
  libxres-dev:arm64 libxshmfence-dev:arm64 libxt-dev:arm64 \
  libxtst-dev:arm64 libxv-dev:arm64 libxvmc-dev:arm64 \
  libxxf86vm-dev:arm64 libxpm-dev:arm64 libxmu-dev:arm64 \
  libxmuu-dev:arm64 libxinerama-dev:arm64 libxaw7-dev:arm64 \
  libxcb-util0-dev:arm64 libxcb-render0-dev:arm64 \
  libxcb-render-util0-dev:arm64 libxcb-randr0-dev:arm64 \
  libxcb-keysyms1-dev:arm64 libxcb-image0-dev:arm64 \
  libxcb-icccm4-dev:arm64 libxcb-glx0-dev:arm64 \
  libxcb-dri3-dev:arm64 libxcb-dri2-0-dev:arm64 \
  libxcb-damage0-dev:arm64 libxcb-xinput-dev:arm64 \
  libxcb-xf86dri0-dev:arm64 libxcb-sync-dev:arm64 \
  libx11-xcb-dev:arm64 libxkbcommon0:arm64 \
  libfontenc-dev:arm64 libfreetype6-dev:arm64 libpng-dev:arm64 \
  liblz4-dev:arm64 libjpeg62-turbo-dev:arm64 libssl-dev:arm64 \
  x11-xkb-utils xfonts-utils xkb-data xtrans-dev xutils-dev xxd autoconf automake libtool

cd /root

# Build custom dependencies using ARM64 cross file
# Resolve CROSS_FILE path
if [ -f "$(pwd)/.gitlab-ci/arm64-cross.txt" ]; then
  CROSS_FILE="$(pwd)/.gitlab-ci/arm64-cross.txt"
else
  CROSS_FILE="/__w/xWebVFB/xWebVFB/.gitlab-ci/arm64-cross.txt"
fi

git clone https://gitlab.freedesktop.org/mesa/drm --depth 1 --branch=libdrm-2.4.126
cd drm
meson _build --cross-file "$CROSS_FILE"
ninja -C _build -j$(nproc) install
cd .. && rm -rf drm

git clone https://gitlab.freedesktop.org/xorg/lib/libxcvt.git --depth 1 --branch=libxcvt-0.1.0
cd libxcvt
meson _build --cross-file "$CROSS_FILE"
ninja -C _build -j$(nproc) install
cd .. && rm -rf libxcvt

git clone https://gitlab.freedesktop.org/xorg/proto/xorgproto.git --depth 1 --branch=xorgproto-2024.1
cd xorgproto
meson setup builddir --cross-file "$CROSS_FILE"
meson compile -C builddir
meson install -C builddir
cd .. && rm -rf xorgproto

git clone https://gitlab.freedesktop.org/wayland/wayland.git --depth 1 --branch=1.21.0
cd wayland
meson -Dtests=false -Ddocumentation=false -Ddtd_validation=false _build --cross-file "$CROSS_FILE"
ninja -C _build -j$(nproc) install
cd .. && rm -rf wayland

git clone https://gitlab.freedesktop.org/wayland/wayland-protocols.git --depth 1 --branch=1.38
cd wayland-protocols
meson _build --cross-file "$CROSS_FILE"
ninja -C _build -j$(nproc) install
cd .. && rm -rf wayland-protocols

git clone https://gitlab.freedesktop.org/libdecor/libdecor.git --depth 1 --branch=0.1.1
cd libdecor
meson _build -Ddemo=false -Dinstall_demo=false --cross-file "$CROSS_FILE"
ninja -C _build -j$(nproc) install
cd .. && rm -rf libdecor

git clone https://gitlab.freedesktop.org/libinput/libei.git --depth 1 --branch=1.0.0
cd libei
meson setup _build -Dtests=disabled -Ddocumentation=[] -Dliboeffis=enabled --cross-file "$CROSS_FILE"
ninja -C _build -j$(nproc) install
cd .. && rm -rf libei