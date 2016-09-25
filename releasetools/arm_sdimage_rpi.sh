#!/usr/bin/env bash
set -e

#
# This script creates a bootable image and should at some point in the future
# be replaced by the proper NetBSD infrastructure.
#

#
# Source settings if present
#
: ${SETTINGS_MINIX=.settings}
if [ -f "${SETTINGS_MINIX}"  ]
then
	echo "Sourcing settings from ${SETTINGS_MINIX}"
	# Display the content (so we can check in the build logs
	# what the settings contain.
	cat ${SETTINGS_MINIX} | sed "s,^,CONTENT ,g"
	. ${SETTINGS_MINIX}
fi

BSP_NAME=rpi
: ${ARCH=evbearm-el}
: ${TOOLCHAIN_TRIPLET=arm-elf32-minix-}
: ${BUILDSH=build.sh}

: ${SETS="minix-base"}
: ${IMG=minix_arm_sd_rpi.img}

# ARM definitions:
: ${BUILDVARS=-V MKGCCCMDS=yes -V MKLLVM=no}
# These BUILDVARS are for building with LLVM:
#: ${BUILDVARS=-V MKLIBCXX=no -V MKKYUA=no -V MKATF=no -V MKLLVMCMDS=no}

if [ ! -f ${BUILDSH} ]
then
	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

# we create a disk image of about 2 gig's
# for alignment reasons, prefer sizes which are multiples of 4096 bytes
: ${FAT_START=4096}
: ${FAT_SIZE=$((    64*(2**20) - ${FAT_START} ))}
: ${ROOT_SIZE=$((   64*(2**20) ))}
: ${HOME_SIZE=$((  128*(2**20) ))}
: ${USR_SIZE=$((  1792*(2**20) ))}
#: ${IMG_SIZE=$((     2*(2**30) ))} # no need to build an image that big for now
: ${IMG_SIZE=$((    64*(2**20) ))}

# set up disk creation environment
. releasetools/image.defaults
. releasetools/image.functions

${RELEASETOOLSDIR}/checkout_repo.sh -o ${RELEASETOOLSDIR}/rpi-firmware -b ${RPI_FIRMWARE_BRANCH} -n ${RPI_FIRMWARE_REVISION} ${RPI_FIRMWARE_URL}

# where the kernel & boot modules will be
MODDIR=${DESTDIR}/boot/minix/.temp

echo "Building work directory..."
build_workdir "$SETS"

# IMG might be a block device
if [ -f ${IMG} ]
then
	rm -f ${IMG}
fi
dd if=/dev/zero of=${IMG} bs=512 count=1 seek=$((($IMG_SIZE / 512) -1)) 2>/dev/null

#
# Generate /root, /usr and /home partition images.
#
echo "Writing disk image..."

#
# Write FAT bootloader partition
#
echo " * BOOT"
rm -rf ${ROOT_DIR}/*
# copy over all modules
for i in ${MODDIR}/*
do
	cp $i ${ROOT_DIR}/$(basename $i).elf
done
${CROSS_PREFIX}objcopy ${OBJ}/minix/kernel/kernel -O binary ${ROOT_DIR}/kernel.bin
# create packer
${CROSS_PREFIX}as ${RELEASETOOLSDIR}/rpi-bootloader/bootloader2.S -o ${RELEASETOOLSDIR}/rpi-bootloader/bootloader2.o
${CROSS_PREFIX}as ${RELEASETOOLSDIR}/rpi-bootloader/bootloader3.S -o ${RELEASETOOLSDIR}/rpi-bootloader/bootloader3.o
${CROSS_PREFIX}ld ${RELEASETOOLSDIR}/rpi-bootloader/bootloader2.o -o ${RELEASETOOLSDIR}/rpi-bootloader/bootloader2.elf -Ttext=0x8000 2> /dev/null
${CROSS_PREFIX}ld ${RELEASETOOLSDIR}/rpi-bootloader/bootloader3.o -o ${RELEASETOOLSDIR}/rpi-bootloader/bootloader3.elf -Ttext=0x8000 2> /dev/null
${CROSS_PREFIX}objcopy -O binary ${RELEASETOOLSDIR}/rpi-bootloader/bootloader2.elf ${ROOT_DIR}/minix_rpi2.bin
${CROSS_PREFIX}objcopy -O binary ${RELEASETOOLSDIR}/rpi-bootloader/bootloader3.elf ${ROOT_DIR}/minix_rpi3.bin
# pack modules
(cd ${ROOT_DIR} && cat <<EOF | cpio -o --format=newc >> ${ROOT_DIR}/minix_rpi2.bin 2>/dev/null
kernel.bin
mod01_ds.elf
mod02_rs.elf
mod03_pm.elf
mod04_sched.elf
mod05_vfs.elf
mod06_memory.elf
mod07_tty.elf
mod08_mib.elf
mod09_vm.elf
mod10_pfs.elf
mod11_mfs.elf
mod12_init.elf
EOF
)
(cd ${ROOT_DIR} && cat <<EOF | cpio -o --format=newc >> ${ROOT_DIR}/minix_rpi3.bin 2>/dev/null
kernel.bin
mod01_ds.elf
mod02_rs.elf
mod03_pm.elf
mod04_sched.elf
mod05_vfs.elf
mod06_memory.elf
mod07_tty.elf
mod08_mib.elf
mod09_vm.elf
mod10_pfs.elf
mod11_mfs.elf
mod12_init.elf
EOF
)
cp -r releasetools/rpi-firmware/* ${ROOT_DIR}

# Write GPU config file
cat <<EOF >${ROOT_DIR}/config.txt
[pi3]
kernel=minix_rpi3.bin
enable_uart=1
dtoverlay=pi3-disable-bt

[pi2]
kernel=minix_rpi2.bin
EOF

${CROSS_TOOLS}/nbmakefs -t msdos -s $FAT_SIZE -O $FAT_START -o "F=32,c=1" ${IMG} ${ROOT_DIR} >/dev/null

#
# Write the partition table using the natively compiled
# minix partition utility
#
${CROSS_TOOLS}/nbpartition -f -m ${IMG} $((${FAT_START}/512)) "c:$((${FAT_SIZE}/512))*"

echo ""
echo "Disk image at `pwd`/${IMG}"
echo ""
echo "To boot this image on kvm:"
echo "qemu-system-arm -M raspi2 -drive if=sd,cache=writeback,format=raw,file=$(pwd)/${IMG} -bios ${ROOT_DIR}/minix_rpi2.bin"
