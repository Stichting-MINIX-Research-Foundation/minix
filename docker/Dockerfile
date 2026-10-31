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
 && rm -rf /var/lib/apt/lists/*

# Harness + ports entry assumptions live under the mounted workspace.
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /work
