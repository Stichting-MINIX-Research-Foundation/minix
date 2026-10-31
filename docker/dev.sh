#!/bin/sh
# docker/dev.sh - front-end for developing in the minix-dev container.
#
#   docker/dev.sh image              (re)build the container image
#   docker/dev.sh shell              interactive bash in the workspace
#   docker/dev.sh <cmd> [args...]    run a command in the container
#
# The workspace (the directory containing minix/, minix-ports/, obj*.i386/)
# is bind-mounted at its own absolute path, so inside==outside paths and
# file ownership is your normal user.
#
# Examples:
#   docker/dev.sh sh build.sh -m i386 -O ../obj.docker.i386 -U -u tools
#   docker/dev.sh ./releasetools/x86_qemu.py --cmd 'uname -a'
#   docker/dev.sh ../minix-ports/build-toolchain.sh gcc

set -eu

HERE=$(cd "$(dirname "$0")" && pwd)          # <repo>/docker
REPO=$(cd "$HERE/.." && pwd)                 # <repo>   (minix checkout)
ROOT=$(cd "$REPO/.." && pwd)                 # workspace root (has minix/, obj*, minix-ports/)
IMG=minix-dev

case "${1:-}" in
    image)
        exec docker build -t "$IMG" "$HERE"
        ;;
    shell)
        set -- bash -l
        ;;
    "")
        echo "usage: dev.sh image|shell|<cmd...>" >&2
        exit 2
        ;;
esac

# KVM when available (harness speed); harmless to omit.
KVMARGS=
if [ -c /dev/kvm ] && [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
    KVMARGS="--device /dev/kvm"
fi

# Figure out CWD mapped into the container (only sensible inside ROOT).
PWD_DIR=$(pwd -P)
case "$PWD_DIR" in
    "$ROOT"*) ;;
    *) PWD_DIR="$REPO" ;;
esac

TTYARGS=
[ -t 0 ] && TTYARGS=-it

exec docker run --rm $TTYARGS \
    $KVMARGS \
    -e HOME=/tmp/minix-dev-home \
    -e MAKEFLAGS \
    --user "$(id -u):$(id -g)" \
    -v "$ROOT:$ROOT" \
    -w "$PWD_DIR" \
    "$IMG" "$@"
