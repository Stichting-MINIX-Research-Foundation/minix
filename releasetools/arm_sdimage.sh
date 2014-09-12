#!/usr/bin/env bash
set -e

#
# This script creates a bootable image and should at some point in the future
# be replaced by makefs.
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
: ${CROSS_TOOLS=${OBJ}/"tooldir.`uname -s`-`uname -r`-`uname -m`"/bin}
: ${CROSS_PREFIX=${CROSS_TOOLS}/arm-elf32-minix-}
: ${JOBS=1}
: ${DESTDIR=${OBJ}/destdir.$ARCH}
: ${RELEASETOOLSDIR=./releasetools/}
: ${FSTAB=${DESTDIR}/etc/fstab}
: ${BUILDVARS=-V MKGCCCMDS=yes -V MKLLVM=no}
: ${BUILDSH=build.sh}
: ${CREATE_IMAGE_ONLY=0}
: ${RC=minix_x86.rc}

# Where the kernel & boot modules will be
MODDIR=${DESTDIR}/boot/minix/.temp

#
# Directory where to store temporary file system images
#
: ${IMG_DIR=${OBJ}/img}
: ${MLO=MLO}
: ${UBOOT=u-boot.img}

# Beagleboard-xm
: ${U_BOOT_BIN_DIR=build/omap3_beagle/}
: ${CONSOLE=tty02}

# BeagleBone (and black)
#: ${U_BOOT_BIN_DIR=build/am335x_evm/}
#: ${CONSOLE=tty00}

#
# We host u-boot binaries.
#
U_BOOT_GIT_VERSION=cb5178f12787c690cb1c888d88733137e5a47b15

#
# All sized are written in 512 byte blocks
#
# we create a disk image of about 2 gig's
# for alignment reasons, prefer sizes which are multiples of 4096 bytes
#
: ${IMG_SIZE=$((     2*(2**30) / 512))}
: ${FAT_SIZE=$((    10*(2**20) / 512))}
: ${ROOT_SIZE=$((   64*(2**20) / 512))}
: ${HOME_SIZE=$((  128*(2**20) / 512))}
: ${USR_SIZE=$((  1792*(2**20) / 512))}

#
# Do some math to determine the start addresses of the partitions.
# Don't leave holes so the 'partition' invocation later is easy.
#
FAT_START=2048
ROOT_START=$(($FAT_START + $FAT_SIZE))
USR_START=$(($ROOT_START + $ROOT_SIZE))
HOME_START=$(($USR_START + $USR_SIZE))

case $(uname -s) in
Darwin)
	MKFS_VFAT_CMD=newfs_msdos
	MKFS_VFAT_OPTS='-h 64 -u 32 -S 512 -s ${FAT_SIZE} -o 0'
;;
FreeBSD)
	MKFS_VFAT_CMD=newfs_msdos
	MKFS_VFAT_OPTS=
;;
*)
	MKFS_VFAT_CMD=mkfs.vfat
	MKFS_VFAT_OPTS=
;;
esac

if [ -n "$BASE_URL" ]
then
	#we no longer download u-boot but do a checkout
	#BASE_URL used to be the base url for u-boot
	#Downloads
	echo "Warning:** Setting BASE_URL (u-boot) is no longer possible use U_BOOT_BIN_DIR"
	echo "Look in ${RELEASETOOLSDIR}/arm_sdimage.sh for suggested values"
	exit 1
fi

if [ ! -f ${BUILDSH} ]
then
	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:${PATH}

for needed in mcopy dd ${MKFS_VFAT_CMD} git
do
	if ! which $needed 2>&1 > /dev/null
	then
		echo "**Skipping image creation: missing tool '$needed'"
		exit 1
	fi
done

: ${IMG=minix_arm_sd.img}

#
# Are we going to build the minix sources?
#

if [ ${CREATE_IMAGE_ONLY} -eq 1 ]
then
	if [ ! -d ${DESTDIR} ]
	then
		echo "Minix source code does'nt appear to have been built."
		echo "Please try with \$CREATE_IMAGE_ONLY set to 0."
		exit 1
	fi
fi

#
# Artifacts from this script are stored in the IMG_DIR
#
rm -rf ${IMG_DIR}

if [ -f ${IMG} ]	# IMG might be a block device
then	rm -f ${IMG}
fi

mkdir -p ${IMG_DIR}

#
# Download the stage 1 bootloader  and u-boot
#
${RELEASETOOLSDIR}/fetch_u-boot.sh -o ${RELEASETOOLSDIR}/u-boot -n $U_BOOT_GIT_VERSION
cp ${RELEASETOOLSDIR}/u-boot/${U_BOOT_BIN_DIR}/u-boot.img ${IMG_DIR}/
cp ${RELEASETOOLSDIR}/u-boot/${U_BOOT_BIN_DIR}/MLO ${IMG_DIR}/

if [ ${CREATE_IMAGE_ONLY} -eq 0 ]
then
	echo "Going to build Minix source code..."
	#
	# Remove the generated files to allow us call build.sh without '-V SLOPPY_FLIST=yes'.
	#
	rm -f ${FSTAB}

	#
	# Now start the build.
	#
	sh ${BUILDSH} -j ${JOBS} -m ${ARCH} -O ${OBJ} -D ${DESTDIR} ${BUILDVARS} -U -u distribution

fi

#
# create a fstab entry in /etc this is normally done during the
# setup phase on x86
#
cat >${FSTAB} <<END_FSTAB
/dev/c0d0p2	/usr	mfs	rw			0	2
/dev/c0d0p3	/home	mfs	rw			0	2
none		/sys	devman	rw,rslabel=devman	0	0
END_FSTAB

rm -f ${DESTDIR}/SETS.*

${CROSS_TOOLS}/nbpwd_mkdb -V 0 -p -d ${DESTDIR} ${DESTDIR}/etc/master.passwd

#
# make the different file system. this part is *also* hacky. We first convert
# the METALOG.sanitised using mtree into a input METALOG containing uids and
# gids.
# After that we do some magic processing to add device nodes (also missing from METALOG)
# and convert the METALOG into a proto file that can be used by mkfs.mfs
#
echo "Creating the file systems"

#
# read METALOG and use mtree to convert the user and group names into uid and gids
# FIX put "input somewhere clean"
#
cat ${DESTDIR}/METALOG.sanitised | ${CROSS_TOOLS}/nbmtree -N ${DESTDIR}/etc -C -K device > ${IMG_DIR}/input

# add rc (if any)
if [ -f ${RC} ]; then
    cp ${RC} ${DESTDIR}/usr/etc/rc.local
    echo "./usr/etc/rc.local type=file uid=0 gid=0 mode=0644" >> ${IMG_DIR}/input
fi

# add fstab
echo "./etc/fstab type=file uid=0 gid=0 mode=0755 size=747 time=1365060731.000000000" >> ${IMG_DIR}/input

# fill root.img (skipping /usr entries while keeping the /usr directory)
cat ${IMG_DIR}/input  | grep -v "^./usr/" | ${CROSS_TOOLS}/nbtoproto -b ${DESTDIR} -o ${IMG_DIR}/root.proto

#
# Create proto files for /usr and /home using toproto.
#
cat ${IMG_DIR}/input  | grep  "^\./usr/\|^. "  | sed "s,\./usr,\.,g" | ${CROSS_TOOLS}/nbtoproto -b ${DESTDIR}/usr -o ${IMG_DIR}/usr.proto
cat ${IMG_DIR}/input  | grep  "^\./home/\|^. "  | sed "s,\./home,\.,g" | ${CROSS_TOOLS}/nbtoproto -b ${DESTDIR}/home -o ${IMG_DIR}/home.proto

#
# Create the FAT partition, which contains the bootloader files, kernel and modules
#
dd if=/dev/zero of=${IMG_DIR}/fat.img bs=512 count=1 seek=$(($FAT_SIZE -1)) 2>/dev/null

#
# Format the fat partition and put the bootloaders
# uEnv and the kernel command line in the FAT partition
#
${MKFS_VFAT_CMD} ${MKFS_VFAT_OPTS} ${IMG_DIR}/fat.img

#
# Create a uEnv.txt file
# -n default to network boot
# -p add a prefix to the network booted files (e.g. xm/"
# -c set console e.g. tty02 or tty00
# -v set verbosity e.g. 0 to 3
#${RELEASETOOLSDIR}/gen_uEnv.txt.sh -c ${CONSOLE} -n -p bb/ > ${IMG_DIR}/uEnv.txt
${RELEASETOOLSDIR}/gen_uEnv.txt.sh -c ${CONSOLE}  > ${IMG_DIR}/uEnv.txt

echo "Copying configuration kernel and boot modules"
mcopy -bsp -i ${IMG_DIR}/fat.img  ${IMG_DIR}/$MLO ::MLO
mcopy -bsp -i ${IMG_DIR}/fat.img ${IMG_DIR}/$UBOOT ::u-boot.img
mcopy -bsp -i ${IMG_DIR}/fat.img ${IMG_DIR}/uEnv.txt ::uEnv.txt

#
# Do some last processing of the kernel and servers and then put them on the FAT
# partition.
#
${CROSS_PREFIX}objcopy ${OBJ}/minix/kernel/kernel -O binary ${OBJ}/kernel.bin

mcopy -bsp -i ${IMG_DIR}/fat.img ${OBJ}/kernel.bin ::kernel.bin

for f in servers/vm/vm servers/rs/rs servers/pm/pm servers/sched/sched \
	servers/vfs/vfs servers/ds/ds fs/mfs/mfs fs/pfs/pfs \
	../sbin/init/init
do
    fn=`basename $f`.elf
    cp ${OBJ}/minix/${f} ${OBJ}/${fn}
    ${CROSS_PREFIX}strip -s ${OBJ}/${fn}
    mcopy -bsp -i ${IMG_DIR}/fat.img  ${OBJ}/${fn} ::${fn}
done

for f in tty/tty/tty storage/memory/memory
do
    fn=`basename $f`.elf
    cp ${OBJ}/minix/drivers/${f} ${OBJ}/${fn}
    ${CROSS_PREFIX}strip -s ${OBJ}/${fn}
    mcopy -bsp -i ${IMG_DIR}/fat.img  ${OBJ}/${fn} ::${fn}
done

#
# For tftp booting
#
cp ${IMG_DIR}/uEnv.txt ${OBJ}/

#
# Create the empty image where we later will put the partitions in.
# Make sure it is at least 2GB, otherwise the SD card will not be detected
# correctly in qemu / HW.
#
dd if=/dev/zero of=${IMG} bs=512 count=1 seek=$(($IMG_SIZE -1))


#
# Generate /root, /usr and /home partition images.
#
echo "Writing Minix filesystem images"
echo " - ROOT"
_ROOT_SIZE=$((`${CROSS_TOOLS}/nbmkfs.mfs -I $((${ROOT_START} * 512)) -b $((${ROOT_SIZE} / 8)) ${IMG} ${IMG_DIR}/root.proto`/512))
echo " - USR"
_USR_SIZE=$((`${CROSS_TOOLS}/nbmkfs.mfs -I $((${USR_START} * 512)) -b $((${USR_SIZE} / 8))  ${IMG}  ${IMG_DIR}/usr.proto`/512))
echo " - HOME"
_HOME_SIZE=$((`${CROSS_TOOLS}/nbmkfs.mfs -I $((${HOME_START} * 512)) -b $((${HOME_SIZE} / 8)) ${IMG} ${IMG_DIR}/home.proto`/512))

#
# Write the partition table using the natively compiled
# minix partition utility
#
${CROSS_TOOLS}/nbpartition -f -m ${IMG} ${FAT_START} "c:${FAT_SIZE}*" \
	81:${_ROOT_SIZE} 81:${_USR_SIZE} 81:${_HOME_SIZE}

#
# Merge the partitions into a single image.
#
echo "Merging file systems"
dd if=${IMG_DIR}/fat.img of=${IMG} seek=$FAT_START conv=notrunc
