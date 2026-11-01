# minix development container
#
# Everything needed for the Hyprland-on-MINIX workflow:
#   * NetBSD build.sh (host tools: nbmake etc.)
#   * releasetools image build
#   * the i586-elf32-minix cross toolchain build (../minix-ports)
#   * the headless QEMU harness (releasetools/x86_qemu.py)
#
# IMPORTANT (ABI rule): artifacts built INSIDE this container run both in the
# container and on the host (old-glibc -> newer-glibc is forward-compatible).
# Host-built binaries may NOT run inside the container. Therefore: always
# build inside the container. The container uses its own obj dir
# (../obj.docker.i386) for the OS build; pass -O ../obj.docker.i386 to
# build.sh (the image script accepts OBJ).
#
# Build:  docker/dev.sh image
# Use:    docker/dev.sh shell            # interactive
#         docker/dev.sh <cmd...>         # any build/harness command

FROM debian:12-slim

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential gcc g++ make \
    bison byacc flex m4 texinfo file gawk sed grep \
    libgmp-dev libmpfr-dev libmpc-dev zlib1g-dev \
    qemu-system-x86 qemu-utils \
    e2fsprogs \
    python3 \
    curl ca-certificates xz-utils bzip2 \
    git rsync cpio \
    ninja-build pkg-config cmake python3-venv libexpat1-dev \
 && rm -rf /var/lib/apt/lists/*

# Ports (minix-ports: libdrm, wayland, wlroots, mesa, ...) need a recent
# meson and mako (mesa); Debian 12's meson is too old for some of them.
RUN python3 -m venv /opt/py \
 && /opt/py/bin/pip install --no-cache-dir meson==1.5.2 mako==1.3.5 \
 && ln -s /opt/py/bin/meson /usr/local/bin/meson

# Harness + ports entry assumptions live under the mounted workspace.
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /work
