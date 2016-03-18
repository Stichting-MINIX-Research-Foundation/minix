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
: ${IMG=minix_x86_usb.img}

if [ ! -f ${BUILDSH} ]
then
	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

# set up disk creation environment
. releasetools/image.defaults
. releasetools/image.functions

: ${RAMDISK_SIZE=$(( 200*(2**20) / 512 / 8 ))}

# where the kernel & boot modules will be
MODDIR=${DESTDIR}/boot/minix/.temp

echo "Fetching syslinux..."
${RELEASETOOLSDIR}/fetch_syslinux.sh -o ${RELEASETOOLSDIR}/syslinux -n $SYSLINUX_GIT_VERSION

echo "Building work directory..."
build_workdir "$SETS"

echo "Adding extra files..."

# create a fstab entry in /etc
cat >${ROOT_DIR}/etc/fstab <<END_FSTAB
none		/sys		devman	rw,rslabel=devman	0	0
none		/dev/pts	ptyfs	rw,rslabel=ptyfs	0	0
END_FSTAB
add_file_spec "etc/fstab" extra.fstab
cp minix/drivers/storage/ramdisk/rc ${ROOT_DIR}/etc/rc.ramdisk
add_file_spec "etc/rc.ramdisk" extra.fstab
cp releasetools/release/pxe/etc/issue ${ROOT_DIR}/etc/issue
add_file_spec "etc/issue" extra.cdfiles

echo "Bundling packages..."
bundle_packages "$BUNDLE_PACKAGES"

echo "Creating specification files..."
create_input_spec
create_protos

echo "Writing ramdisk image..."
cp ${MODDIR}/* ${WORK_DIR}
create_ramdisk_image ${RAMDISK_SIZE}

echo "Preparing PXE boot directory..."
mkdir ${WORK_DIR}/pxelinux.cfg
cat >${WORK_DIR}/pxelinux.cfg/default <<EOF
DEFAULT menu.c32
TIMEOUT 100

LABEL x86
  MENU LABEL Minix3
  KERNEL mboot.c32
  APPEND kernel bootramdisk=1 --- mod01_ds --- mod02_rs --- mod03_pm --- mod04_sched --- mod05_vfs --- mod06_memory --- mod07_tty --- mod08_mib --- mod09_vm --- mod10_pfs --- mod11_mfs --- mod12_init
EOF
cp ${RELEASETOOLSDIR}/syslinux/bios/com32/elflink/ldlinux/ldlinux.c32 ${WORK_DIR}/
cp ${RELEASETOOLSDIR}/syslinux/bios/com32/lib/libcom32.c32 ${WORK_DIR}/
cp ${RELEASETOOLSDIR}/syslinux/bios/com32/libutil/libutil.c32 ${WORK_DIR}/
cp ${RELEASETOOLSDIR}/syslinux/bios/com32/mboot/mboot.c32 ${WORK_DIR}/
cp ${RELEASETOOLSDIR}/syslinux/bios/com32/menu/menu.c32 ${WORK_DIR}/
cp ${RELEASETOOLSDIR}/syslinux/bios/core/pxelinux.0 ${WORK_DIR}/
rm -rf ${WORK_DIR}/extra.* ${WORK_DIR}/set.* ${WORK_DIR}/proto.* ${WORK_DIR}/imgrd.mfs ${WORK_DIR}/imgrd.o

echo "PXE boot directory at ${WORK_DIR}"
echo "To boot this image on kvm:"
echo "cd ${WORK_DIR} && qemu-system-i386 --enable-kvm -m 512M -tftp . -bootp pxelinux.0 -boot n"
