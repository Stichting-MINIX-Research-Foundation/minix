#!/usr/bin/env bash
set -e

#
# This script creates a bootable image and should at some point in the future
# be replaced by the proper NetBSD infrastructure.
#
# Supported command line switches:
#   -i   build iso image instead of qemu imaeg
#   -b   bitcode build, increase partition sizes as necessary
#

: ${ARCH=i386}
: ${OBJ=../obj.${ARCH}}
: ${TOOLCHAIN_TRIPLET=i586-elf32-minix-}
: ${BUILDSH=build.sh}

: ${SETS="minix-base minix-comp minix-games minix-man minix-tests tests"}
: ${IMG=minix_x86.img}

if [ ! -f ${BUILDSH} ]
then
	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

# we create a disk image of about 2 gig's
# for alignment reasons, prefer sizes which are multiples of 4096 bytes
: ${BOOTXX_SECS=32}
: ${ROOT_SIZE=$((  128*(2**20) - ${BOOTXX_SECS} * 512 ))}
: ${HOME_SIZE=$((  128*(2**20) ))}
: ${USR_SIZE=$((  1792*(2**20) ))}

# set up disk creation environment
. releasetools/image.defaults
. releasetools/image.functions

# all sizes are written in 512 byte blocks
ROOTSIZEARG="-b $((${ROOT_SIZE} / 512 / 8))"
USRSIZEARG="-b $((${USR_SIZE} / 512 / 8))"
HOMESIZEARG="-b $((${HOME_SIZE} / 512 / 8))"

# where the kernel & boot modules will be
MODDIR=${DESTDIR}/boot/minix/.temp

echo "Building work directory..."
build_workdir "$SETS"

echo "Adding extra files..."

# create a fstab entry in /etc
cat >${ROOT_DIR}/etc/fstab <<END_FSTAB
/dev/c0d0p1	/usr		mfs	rw			0	2
/dev/c0d0p2	/home		mfs	rw			0	2
none		/sys		devman	rw,rslabel=devman	0	0
none		/dev/pts	ptyfs	rw,rslabel=ptyfs	0	0
END_FSTAB
add_file_spec "etc/fstab" extra.fstab

cp ${DESTDIR}/usr/mdec/boot_monitor ${ROOT_DIR}/boot_monitor
add_file_spec "boot_monitor" extra.boot

add_link_spec "boot/minix_latest" "minix_default" extra.kernel
workdir_add_kernel minix_default
workdir_add_kernel minix/$RELEASE_VERSION

# add boot.cfg
cat >${ROOT_DIR}/boot.cfg <<END_BOOT_CFG
clear=1
timeout=5
default=2
menu=Start MINIX 3:load_mods /boot/minix_default/mod*; multiboot /boot/minix_default/kernel rootdevname=c0d0p0
menu=Start latest MINIX 3:load_mods /boot/minix_latest/mod*; multiboot /boot/minix_latest/kernel rootdevname=c0d0p0
menu=Start latest MINIX 3 in single user mode:load_mods /boot/minix_latest/mod*; multiboot /boot/minix_latest/kernel rootdevname=c0d0p0 bootopts=-s
menu=Edit menu option:edit
menu=Drop to boot prompt:prompt
default=2
menu=Start MINIX 3 ($RELEASE_VERSION):load_mods /boot/minix/$RELEASE_VERSION/mod*; multiboot /boot/minix/$RELEASE_VERSION/kernel rootdevname=c0d0p0
END_BOOT_CFG
add_file_spec "boot.cfg" extra.boot

echo "Bundling packages..."
bundle_packages "$BUNDLE_PACKAGES"

echo "Creating specification files..."
create_input_spec
create_protos "usr home"

# Clean image
if [ -f ${IMG} ]	# IMG might be a block device
then
	rm -f ${IMG}
fi

#
# Generate /root, /usr and /home partition images.
#
echo "Writing disk image..."
ROOT_START=${BOOTXX_SECS}
echo " * ROOT"
_ROOT_SIZE=$(${CROSS_TOOLS}/nbmkfs.mfs -d ${ROOTSIZEARG} -I $((${ROOT_START}*512)) ${IMG} ${WORK_DIR}/proto.root)
_ROOT_SIZE=$(($_ROOT_SIZE / 512))
USR_START=$((${ROOT_START} + ${_ROOT_SIZE}))
echo " * USR"
_USR_SIZE=$(${CROSS_TOOLS}/nbmkfs.mfs  -d ${USRSIZEARG}  -I $((${USR_START}*512))  ${IMG} ${WORK_DIR}/proto.usr)
_USR_SIZE=$(($_USR_SIZE / 512))
HOME_START=$((${USR_START} + ${_USR_SIZE}))
echo " * HOME"
_HOME_SIZE=$(${CROSS_TOOLS}/nbmkfs.mfs -d ${HOMESIZEARG} -I $((${HOME_START}*512)) ${IMG} ${WORK_DIR}/proto.home)
_HOME_SIZE=$(($_HOME_SIZE / 512))

#
# Write the partition table using the natively compiled
# minix partition utility
#
${CROSS_TOOLS}/nbpartition -m ${IMG} ${BOOTXX_SECS} 81:${_ROOT_SIZE} 81:${_USR_SIZE} 81:${_HOME_SIZE}
${CROSS_TOOLS}/nbinstallboot -f -m ${ARCH} ${IMG} ${DESTDIR}/usr/mdec/bootxx_minixfs3


mods="`( cd ${MODDIR}; echo mod* | tr ' ' ',' )`"
echo "Disk image at `pwd`/${IMG}"
echo "To boot this image on kvm:"
echo "cd ${MODDIR} && qemu-system-i386 --enable-kvm -m 256 -kernel kernel -append \"rootdevname=c0d0p0\" -initrd \"${mods}\" -hda `pwd`/${IMG}"
