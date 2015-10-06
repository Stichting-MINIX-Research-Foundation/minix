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

if [ ! -f ${BUILDSH} ]
then
	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

#: ${RAMDISK_SIZE=$(( 200*(2**20) ))}

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

echo ""
echo "RAM image modules at ${WORK_DIR}"
echo ""
echo "To boot this image on kvm:"
echo "cd ${WORK_DIR} && qemu-system-i386 --enable-kvm -m 1G -kernel kernel -append \"bootramdisk=1\" -initrd \"${mods}\""
