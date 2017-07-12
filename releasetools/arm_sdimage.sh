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

: ${ARCH=evbearm-el}
: ${OBJ=../obj.${ARCH}}
: ${TOOLCHAIN_TRIPLET=arm-elf32-minix-}
: ${BUILDSH=build.sh}

: ${SETS="minix-base minix-comp minix-games minix-man minix-tests tests"}
: ${IMG=minix_arm_sd.img}

# ARM definitions:
: ${BUILDVARS=-V MKGCCCMDS=yes -V MKLLVM=no}
# These BUILDVARS are for building with LLVM:
#: ${BUILDVARS=-V MKLIBCXX=no -V MKKYUA=no -V MKATF=no -V MKLLVMCMDS=no}
: ${FAT_SIZE=$((    10*(2**20) / 512))} # This is in sectors

# Beagleboard-xm
: ${U_BOOT_BIN_DIR=build/omap3_beagle/}
: ${CONSOLE=tty02}

# BeagleBone (and black)
#: ${U_BOOT_BIN_DIR=build/am335x_evm/}
#: ${CONSOLE=tty00}

#
# We host u-boot binaries.
#
: ${MLO=MLO}
: ${UBOOT=u-boot.img}
U_BOOT_GIT_VERSION=cb5178f12787c690cb1c888d88733137e5a47b15

if [ ! -f ${BUILDSH} ]
then
	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

if [ -n "$BASE_URL" ]
then
	#we no longer download u-boot but do a checkout
	#BASE_URL used to be the base url for u-boot
	#Downloads
	echo "Warning:** Setting BASE_URL (u-boot) is no longer possible use U_BOOT_BIN_DIR"
	echo "Look in ${RELEASETOOLSDIR}/arm_sdimage.sh for suggested values"
	exit 1
fi

export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:${PATH}

# we create a disk image of about 2 gig's
# for alignment reasons, prefer sizes which are multiples of 4096 bytes
: ${IMG_SIZE=$((     2*(2**30) ))}
: ${ROOT_SIZE=$((   64*(2**20) ))}
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
/dev/c0d0p2	/usr		mfs	rw			0	2
/dev/c0d0p3	/home		mfs	rw			0	2
none		/sys		devman	rw,rslabel=devman	0	0
none		/dev/pts	ptyfs	rw,rslabel=ptyfs	0	0
END_FSTAB
add_file_spec "etc/fstab" extra.fstab

echo "Bundling packages..."
bundle_packages "$BUNDLE_PACKAGES"

echo "Creating specification files..."
create_input_spec
create_protos "usr home"

# Download the stage 1 bootloader and u-boot
#
${RELEASETOOLSDIR}/fetch_u-boot.sh -o ${RELEASETOOLSDIR}/u-boot -n $U_BOOT_GIT_VERSION

# Clean image
if [ -f ${IMG} ]	# IMG might be a block device
then
	rm -f ${IMG}
fi

#
# Create the empty image where we later will put the partitions in.
# Make sure it is at least 2GB, otherwise the SD card will not be detected
# correctly in qemu / HW.
#
dd if=/dev/zero of=${IMG} bs=512 count=1 seek=$((($IMG_SIZE / 512) -1))

#
# Generate /root, /usr and /home partition images.
#
echo "Writing disk image..."
FAT_START=2048 # those are sectors
ROOT_START=$(($FAT_START + $FAT_SIZE))
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
echo " * BOOT"
rm -rf ${ROOT_DIR}/*
cp ${RELEASETOOLSDIR}/u-boot/${U_BOOT_BIN_DIR}/MLO ${ROOT_DIR}/
cp ${RELEASETOOLSDIR}/u-boot/${U_BOOT_BIN_DIR}/u-boot.img ${ROOT_DIR}/

# Create a uEnv.txt file
# -n default to network boot
# -p add a prefix to the network booted files (e.g. xm/"
# -c set console e.g. tty02 or tty00
# -v set verbosity e.g. 0 to 3
#${RELEASETOOLSDIR}/gen_uEnv.txt.sh -c ${CONSOLE} -n -p bb/ > ${WORK_DIR}/uEnv.txt
${RELEASETOOLSDIR}/gen_uEnv.txt.sh -c ${CONSOLE}  > ${ROOT_DIR}/uEnv.txt

# Do some last processing of the kernel and servers and then put them on the FAT
# partition.
${CROSS_PREFIX}objcopy ${OBJ}/minix/kernel/kernel -O binary ${ROOT_DIR}/kernel.bin

for f in servers/vm/vm servers/rs/rs servers/pm/pm servers/sched/sched \
	servers/vfs/vfs servers/ds/ds servers/mib/mib fs/pfs/pfs fs/mfs/mfs \
	../sbin/init/init drivers/tty/tty/tty drivers/storage/memory/memory
do
    fn=`basename $f`.elf
    cp ${OBJ}/minix/${f} ${ROOT_DIR}/${fn}
    ${CROSS_PREFIX}strip -s ${ROOT_DIR}/${fn}
done
cat >${WORK_DIR}/boot.mtree <<EOF
. type=dir
./MLO type=file
./u-boot.img type=file
./uEnv.txt type=file
./kernel.bin type=file
./ds.elf type=file
./rs.elf type=file
./pm.elf type=file
./sched.elf type=file
./vfs.elf type=file
./memory.elf type=file
./tty.elf type=file
./mib.elf type=file
./vm.elf type=file
./pfs.elf type=file
./mfs.elf type=file
./init.elf type=file
EOF

#
# Create the FAT partition, which contains the bootloader files, kernel and modules
#
${CROSS_TOOLS}/nbmakefs -t msdos -s ${FAT_SIZE}b -o F=16,c=1 \
	-F ${WORK_DIR}/boot.mtree ${WORK_DIR}/fat.img ${ROOT_DIR}

#
# Write the partition table using the natively compiled
# minix partition utility
#
${CROSS_TOOLS}/nbpartition -f -m ${IMG} ${FAT_START} \
	"c:${FAT_SIZE}*" 81:${_ROOT_SIZE} 81:${_USR_SIZE} 81:${_HOME_SIZE}

#
# Merge the partitions into a single image.
#
echo "Merging file systems"
dd if=${WORK_DIR}/fat.img of=${IMG} seek=$FAT_START conv=notrunc

echo "Disk image at `pwd`/${IMG}"
echo "To boot this image on kvm:"
echo "qemu-system-arm -M beaglexm -serial stdio -drive if=sd,cache=writeback,file=`pwd`/${IMG}"
