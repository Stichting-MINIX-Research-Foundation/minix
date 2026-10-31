MINIX 3.4.0rc6 — amd64 UEFI boot media
===========================================

This CD contains an experimental 64-bit MINIX build for x86_64 PCs.

Boot requirements
-----------------
- An x86_64 processor.
- UEFI firmware with x64 EFI application support.
- Boot the media in UEFI mode. This ISO does not include a legacy BIOS
  boot entry.

Booting
-------
The firmware starts EFI/BOOT/BOOTX64.EFI from the FAT EFI El Torito image.
The MINIX kernel and its boot modules are loaded from the ISO9660 filesystem
using the amd64 Multiboot2 entry point.

The default boot menu starts MINIX from the CD. A serial-console option and
a single-user option are also available.

Release sets
------------
The binary release archives are included in:

  /usr/amd64/binary/sets/

These archives can be used as source material for a manual installation or
for recovery work.

Installation status
-------------------
This is bootable UEFI media, but it is not yet a guided disk installer.
It does not automatically partition a disk, create a GPT layout, or install
the release sets onto a target disk. The UEFI/GPT installer workflow is
still under development.

Do not use this experimental image on a disk containing data you need.
Test it in a virtual machine first, and make backups before attempting any
manual disk operations.

Build information
-----------------
Source: https://github.com/eclairdosesleekman/minix
Architecture: amd64 / x86_64
Firmware: UEFI x64
Kernel interface: Multiboot2
