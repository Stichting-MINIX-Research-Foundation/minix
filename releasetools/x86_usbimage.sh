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

#: ${RAMDISK_SIZE=$(( 200*(2**20) ))}
: ${BOOTXX_SECS=32}

# set up disk creation environment
. releasetools/image.defaults
. releasetools/image.functions

# where the kernel & boot modules will be
MODDIR=${DESTDIR}/boot/minix/.temp

echo "Building work directory..."
build_workdir "$SETS"

echo "Adding extra files..."
workdir_add_ramdisk_files

# set correct message of the day (log in and install tip)
cp releasetools/release/ramdisk/etc/issue ${ROOT_DIR}/etc/issue
add_file_spec "etc/issue" extra.cdfiles

echo "Bundling packages..."
bundle_packages "$BUNDLE_PACKAGES"

echo "Creating specification files..."
create_input_spec
create_protos

echo "Writing ramdisk image..."
# add the other modules for boot
cp ${MODDIR}/* ${WORK_DIR}
create_ramdisk_image ${RAMDISK_SIZE}

echo "Writing USB image..."
# clear ROOT_DIR
rm -rf ${ROOT_DIR}/*
echo ". type=dir uid=0 gid=0 mode=0755" > ${WORK_DIR}/extra.boot

# move all modules back to ROOT_DIR
mv ${WORK_DIR}/kernel ${WORK_DIR}/mod* ${ROOT_DIR}/
add_file_spec "kernel" extra.boot
for i in ${ROOT_DIR}/mod*; do
	add_file_spec $(basename $i) extra.boot
done

# add boot.cfg
cat >${ROOT_DIR}/boot.cfg <<END_BOOT_CFG
menu=Start MINIX 3:load_mods /mod*; multiboot /kernel bootramdisk=1
menu=Edit menu option:edit
menu=Drop to boot prompt:prompt
clear=1
timeout=5
default=1
END_BOOT_CFG
add_file_spec "boot.cfg" extra.boot

# add boot monitor
cp ${DESTDIR}/usr/mdec/boot_monitor ${ROOT_DIR}/boot_monitor
add_file_spec "boot_monitor" extra.boot

# create proto file
cat ${WORK_DIR}/extra.boot | ${CROSS_TOOLS}/nbtoproto -b ${ROOT_DIR} -o ${WORK_DIR}/proto.boot

ROOT_START=${BOOTXX_SECS}
_ROOT_SIZE=$(${CROSS_TOOLS}/nbmkfs.mfs -I $((${ROOT_START} * 512)) ${IMG} ${WORK_DIR}/proto.boot)
_ROOT_SIZE=$(($_ROOT_SIZE / 512))

#
# Write the partition table using the natively compiled
# minix partition utility
#
${CROSS_TOOLS}/nbpartition -m ${IMG} ${BOOTXX_SECS} 81:${_ROOT_SIZE}
${CROSS_TOOLS}/nbinstallboot -f -m ${ARCH} ${IMG} ${DESTDIR}/usr/mdec/bootxx_minixfs3

echo ""
echo "Universally Supported Boot disk image at `pwd`/${IMG}"
echo ""
echo "To boot this image on kvm using the bootloader:"
# This is really, really slow.
# echo "qemu-system-i386 --enable-kvm -m 1G -usbdevice disk:`pwd`/${IMG}"
echo "qemu-system-i386 --enable-kvm -m 1G -hda `pwd`/${IMG}"
echo ""
echo "To boot this image on kvm:"
echo "cd ${ROOT_DIR} && qemu-system-i386 --enable-kvm -m 1G -kernel kernel -append \"bootramdisk=1\" -initrd \"${mods}\""
