#!/bin/bash
set -e

#
# Source settings if present
#
if [ -e .settings ]
then
	echo "Sourcing settings from .settings"
	# Display the content (so we can check in the build logs
	# what the settings contain.
	cat .settings | sed "s,^,CONTENT ,g"
 	. .settings
fi

: ${ARCH=evbearm-el}
: ${OBJ=../obj.${ARCH}}
: ${CROSS_TOOLS=${OBJ}/"tooldir.`uname -s`-`uname -r`-`uname -m`"/bin}
: ${CROSS_PREFIX=${CROSS_TOOLS}/arm-elf32-minix-}
: ${JOBS=1}
: ${DESTDIR=${OBJ}/destdir.$ARCH}
: ${FSTAB=${DESTDIR}/etc/fstab}
: ${BUILDVARS=}
: ${BUILDSH=build.sh}

#
# Directory where to store temporary file system images
#
: ${IMG_DIR=${OBJ}/img}
: ${IMG=minix_arm_sd.img}
: ${MLO=MLO}
: ${UBOOT=u-boot.img}


# beagleboard-xm
: ${BASE_URL=http://www.minix3.org/arm/beagleboard-xm}
: ${FLAG=-DDM37XX}
: ${CONSOLE=tty02}


#beaglebone (and black)
#: ${BASE_URL=http://www.minix3.org/arm/beaglebone}
#: ${FLAG=-DAM335X}
#: ${CONSOLE=tty00}

if [ ! -f ${BUILDSH} ]
then	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:$PATH

for needed in mcopy dd wget mkfs.vfat
do
	if ! which $needed 2>&1 > /dev/null
	then
		echo "**Skipping image creation: missing tool '$needed'"
		exit 1
	fi
done

#
# Artifacts from this script are stored in the IMG_DIR
#
mkdir -p $IMG_DIR

#
# Download the stage 1 bootloader  and u-boot
#
for i in ${MLO} ${UBOOT} 
do
	if [ ! -f ${IMG_DIR}/${i} ]
	then
		if ! wget -O ${IMG_DIR}/$i ${BASE_URL}/$i
		then
			echo "Failed to download $i"
			rm -f ${IMG_DIR}/$i
			exit 1
		fi
		
	fi
done

#
# Call build.sh using a sloppy file list so we don't need to remove the installed /etc/fstag
#
export CPPFLAGS=${FLAG}
sh ${BUILDSH} -V SLOPPY_FLIST=yes -V MKBINUTILS=yes -V MKGCCCMDS=yes -j ${JOBS} -m ${ARCH} -O ${OBJ} -D ${DESTDIR} ${BUILDVARS} -U -u distribution

#
# This script creates a bootable image and should at some point in the future
# be replaced by makefs.
#
# All sized are written in 512 byte blocks
#
# we create a disk image of about 2 gig's
: ${IMG_SIZE=$((     2*(2**30) / 512))}
: ${FAT_SIZE=$((    10*(2**20) / 512))}
: ${ROOT_SIZE=$((   64*(2**20) / 512))}
: ${HOME_SIZE=$((  128*(2**20) / 512))}
: ${USR_SIZE=$((  1536*(2**20) / 512))}

#
# create a fstab entry in /etc this is normally done during the
# setup phase on x86
#
cat >${FSTAB} <<END_FSTAB
/dev/c0d0p2   /home   mfs     rw                      0       2
/dev/c0d0p3   /usr    mfs     rw                      0       2
END_FSTAB

rm -f ${DESTDIR}/SETS.*

${CROSS_TOOLS}/nbpwd_mkdb -V 0 -p -d ${DESTDIR} ${DESTDIR}/etc/master.passwd

#
# Now given the sizes above use DD to create separate files representing
# the partitions we are going to use.
#
dd if=/dev/zero of=${IMG_DIR}/fat.img bs=512 count=1 seek=$(($FAT_SIZE -1)) 2>/dev/null

#
# Create the empty image where we later will put the partitions in.
#
rm -f ${IMG}
dd if=/dev/zero of=${IMG} bs=512 count=1 seek=$(($IMG_SIZE -1))

#
# Do some math to determine the start addresses of the partitions.
# Don't leave holes so the 'partition' invocation later is easy.
#
FAT_START=2048
ROOT_START=$(($FAT_START + $FAT_SIZE))
HOME_START=$(($ROOT_START + $ROOT_SIZE))
USR_START=$(($HOME_START + $HOME_SIZE))

#
# Write the partition table using the natively compiled
# minix partition utility
#
${CROSS_TOOLS}/nbpartition -m ${IMG} ${FAT_START} "c:${FAT_SIZE}*" 81:${ROOT_SIZE} 81:${HOME_SIZE} 81:${USR_SIZE}

#
# Format the fat partition and put the bootloaders
# uEnv and the kernel command line in the FAT partition
#
mkfs.vfat ${IMG_DIR}/fat.img

#
# Create a uEnv.txt file
# -n default to network boot
# -p add a prefix to the network booted files (e.g. xm/"
./releasetools/gen_uEnv.txt.sh > ${IMG_DIR}/uEnv.txt

#
# Generate the MINIX command line
# 
# options:
# -c set console e.g. tty02 or tty00
# -v set verbosity e.g. 0 to 3
./releasetools/gen_cmdline.txt.sh -c ${CONSOLE} > ${IMG_DIR}/cmdline.txt

echo "Copying configuration kernel and boot modules"
mcopy -bsp -i ${IMG_DIR}/fat.img  ${IMG_DIR}/$MLO ::MLO
mcopy -bsp -i ${IMG_DIR}/fat.img ${IMG_DIR}/$UBOOT ::u-boot.img
mcopy -bsp -i ${IMG_DIR}/fat.img ${IMG_DIR}/uEnv.txt ::uEnv.txt
mcopy -bsp -i ${IMG_DIR}/fat.img ${IMG_DIR}/cmdline.txt ::cmdline.txt

#
# For tftp booting
#
cp ${IMG_DIR}/uEnv.txt ${OBJ}/
cp ${IMG_DIR}/cmdline.txt ${OBJ}/

#
# Do some last processing of the kernel and servers before also putting
# them on the FAT
#
${CROSS_PREFIX}objcopy ${OBJ}/kernel/kernel -O binary ${OBJ}/kernel.bin

mcopy -bsp -i ${IMG_DIR}/fat.img ${OBJ}/kernel.bin ::kernel.bin

for f in vm rs pm sched vfs ds mfs pfs init
do
    cp ${OBJ}/servers/${f}/${f} ${OBJ}/${f}.elf
    ${CROSS_PREFIX}strip -s ${OBJ}/${f}.elf
    mcopy -bsp -i ${IMG_DIR}/fat.img  ${OBJ}/${f}.elf ::${f}.elf
done

for f in tty memory
do
    cp ${OBJ}/drivers/${f}/${f} ${OBJ}/${f}.elf
    ${CROSS_PREFIX}strip -s ${OBJ}/${f}.elf
    mcopy -bsp -i ${IMG_DIR}/fat.img  ${OBJ}/${f}.elf ::${f}.elf
done

#
# make the different file system. this part is *also* hacky. We first convert
# the METALOG.sanitised using mtree into a input METALOG containing uids and
# gids.
# Afther that we do some processing to convert the METALOG into a proto file
# that can be used by mkfs.mfs
#
echo "creating the file systems"

#
# read METALOG and use mtree to conver the user and group names into uid and gids
# FIX put "input somwhere clean"
#
cat ${DESTDIR}/METALOG.sanitised | ${CROSS_TOOLS}/nbmtree -N ${DESTDIR}/etc -C -K device > ${IMG_DIR}/input

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
# Generate /root, /usr and /home partition images.
#
echo "Writing Minix filesystem images"
echo " - ROOT"
${CROSS_TOOLS}/nbmkfs.mfs -I $((${ROOT_START} * 512)) -b $((${ROOT_SIZE} / 8)) ${IMG} ${IMG_DIR}/root.proto
echo " - USR"
${CROSS_TOOLS}/nbmkfs.mfs -I $((${USR_START} * 512)) -b $((${USR_SIZE} / 8))  ${IMG}  ${IMG_DIR}/usr.proto
echo " - HOME"
${CROSS_TOOLS}/nbmkfs.mfs -I $((${HOME_START} * 512)) -b $((${HOME_SIZE} / 8)) ${IMG} ${IMG_DIR}/home.proto

#
# Merge the partitions into a single image.
#
echo "Merging file systems"
dd if=${IMG_DIR}/fat.img of=${IMG} seek=$FAT_START conv=notrunc
