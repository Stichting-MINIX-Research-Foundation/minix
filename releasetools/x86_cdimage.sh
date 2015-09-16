#!/usr/bin/env bash
set -e

#
# This script creates a bootable image and should at some point in the future
# be replaced by the proper NetBSD infrastructure.
#

: ${ARCH=i386}
: ${OBJ=../obj.${ARCH}}
: ${TOOLCHAIN_TRIPLET=i586-elf32-minix-}
: ${BUILDSH=build.sh}

: ${SETS="minix-base"}
: ${IMG=minix_x86.iso}

if [ ! -f ${BUILDSH} ]
then
	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

# set up disk creation environment
. releasetools/image.defaults
. releasetools/image.functions

# where the kernel & boot modules will be
MODDIR=${DESTDIR}/boot/minix/.temp

echo "Building work directory..."
build_workdir "$SETS"

echo "Adding extra files..."
workdir_add_sets
workdir_add_cdfiles

# create a fstab entry in /etc
cat >${ROOT_DIR}/etc/fstab <<END_FSTAB
none		/sys		devman	rw,rslabel=devman	0	0
none		/dev/pts	ptyfs	rw,rslabel=ptyfs	0	0
END_FSTAB
add_file_spec "etc/fstab" extra.fstab

workdir_add_kernel minix_default

# add boot.cfg
cat >${ROOT_DIR}/boot.cfg <<END_BOOT_CFG
banner=Welcome to the MINIX 3 installation CD
banner================================================================================
banner=
menu=Regular MINIX 3:multiboot /boot/minix_default/kernel bootcd=1 cdproberoot=1 disable=inet
menu=Regular MINIX 3 (with AHCI):multiboot /boot/minix_default/kernel bootcd=1 cdproberoot=1 disable=inet ahci=yes
menu=Edit menu option:edit
menu=Drop to boot prompt:prompt
clear=1
timeout=10
default=1
load=/boot/minix_default/mod01_ds
load=/boot/minix_default/mod02_rs
load=/boot/minix_default/mod03_pm
load=/boot/minix_default/mod04_sched
load=/boot/minix_default/mod05_vfs
load=/boot/minix_default/mod06_memory
load=/boot/minix_default/mod07_tty
load=/boot/minix_default/mod08_mfs
load=/boot/minix_default/mod09_vm
load=/boot/minix_default/mod10_pfs
load=/boot/minix_default/mod11_init
END_BOOT_CFG
add_file_spec "boot.cfg" extra.cdfiles

# add README.TXT
cp releasetools/release/cd/README.TXT ${ROOT_DIR}/README.TXT
add_file_spec "README.TXT" extra.cdfiles

# set correct message of the day (log in and install tip)
cp releasetools/release/cd/etc/issue ${ROOT_DIR}/etc/issue
add_file_spec "etc/issue" extra.cdfiles

echo "Bundling packages..."
bundle_packages "$BUNDLE_PACKAGES"

echo "Creating specification files..."
create_input_spec
create_protos

# Clean image
if [ -f ${IMG} ]	# IMG might be a block device
then
	rm -f ${IMG}
fi

echo "Writing ISO..."
${CROSS_TOOLS}/nbmakefs -t cd9660 -F ${WORK_DIR}/input -o "rockridge,bootimage=i386;${DESTDIR}/usr/mdec/bootxx_cd9660,label=MINIX" ${IMG} ${ROOT_DIR}

#mods=$(cd ${MODDIR}; echo mod* | tr ' ' ',')

echo "ISO image at `pwd`/${IMG}"
echo "To boot this image on kvm:"
#echo "cd ${MODDIR} && qemu-system-i386 --enable-kvm -kernel kernel -append \"bootcd=1 cdproberoot=1 disable=inet\" -initrd \"${mods}\" -cdrom `pwd`/${IMG}"
echo "qemu-system-i386 --enable-kvm -cdrom `pwd`/${IMG}"
