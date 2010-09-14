#!/bin/sh

if [ ! -f /proc/pci ]; then
  echo "PCI list not found (is /proc mounted?)" >&2
  exit 1
fi

exec cut -d' ' -f3- /proc/pci
