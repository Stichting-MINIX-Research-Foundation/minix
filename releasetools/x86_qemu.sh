#!/bin/sh
# Thin wrapper for the headless QEMU test harness (see x86_qemu.py header).
exec "$(dirname "$0")/x86_qemu.py" "$@"
