MINIX 3.4.0 — amd64 UEFI installation media
============================================

This media contains an experimental 64-bit MINIX build and a guided
installer for x86_64 PCs.

Boot requirements
-----------------
- An x86_64 processor.
- UEFI firmware with x64 EFI application support.
- Boot the media in UEFI mode. This image does not include a legacy BIOS
  boot entry.

Booting and installation
------------------------
The firmware starts EFI/BOOT/BOOTX64.EFI from the FAT EFI El Torito image.
The MINIX kernel and boot modules are loaded from the ISO9660 filesystem
using the amd64 Multiboot2 entry point.

After MINIX starts, the primary console automatically launches the guided
installer. You can cancel at its first prompt and use the root recovery
shell, or select a separate target disk to install MINIX 3.4.0 amd64.

The installer creates the following GPT layout on the selected disk:

  p0   64 MiB   FAT32 EFI System Partition
  p1   rest     MINIX MFS root filesystem

The kernel and boot modules are placed in /boot/minix_default. The EFI
System Partition receives a disk-specific boot image with a menu entry that
loads MINIX from the MFS partition belonging to the UEFI boot device. The
bootloader resolves this partition dynamically; `/etc/fstab` selects the
installed root device. The installation requires a target disk of at least
2048 MiB.

Destructive operation
---------------------
The whole selected target disk is repartitioned. All data and existing
partitions on that disk will be erased. Before writing anything, the
installer displays the selected disk and requires you to type its device
name exactly.

Do not choose the disk from which the installer is currently running. In
particular, when booting the install-image USB, install to a different
disk. The installer does not change the firmware boot order; after the
installation, remove the media and select the target disk's UEFI entry.

The ISO carries /efiboot-disk.img, the FAT image copied to the target EFI
System Partition. Its boot menu uses paths relative to the UEFI boot device,
so it does not depend on the target disk being enumerated as hd0. The
install-image USB instead reuses its existing EFI partition.

Release sets
------------
The binary release archives are included in:

  /usr/amd64/binary/sets/

These are the source archives extracted by the guided installer. The
installer only processes release sets that are present; minix-base.tgz and
minix-kernel.tgz are required.

If installation is cancelled or completes, the installer returns to a root
shell for recovery or additional configuration.

Build information
-----------------
Source: https://github.com/eclairdosesleekman/minix
Architecture: amd64 / x86_64
Firmware: UEFI x64
Disk layout: GPT + EFI System Partition + MINIX MFS
Kernel interface: Multiboot2

This port and installer are experimental. Test in a virtual machine first
and make backups before attempting installation on physical hardware.
