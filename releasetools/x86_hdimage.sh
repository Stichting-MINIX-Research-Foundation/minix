#!/bin/bash
set -e

: ${ARCH=i386}
: ${OBJ=../obj.${ARCH}}
: ${CROSS_TOOLS=${OBJ}/"tooldir.`uname -s`-`uname -r`-`uname -m`"/bin}
: ${CROSS_PREFIX=${CROSS_TOOLS}/i586-elf32-minix-}
: ${JOBS=1}
: ${DESTDIR=${OBJ}/destdir.$ARCH}
: ${FSTAB=${DESTDIR}/etc/fstab}
: ${BUILDVARS=}
: ${BUILDSH=build.sh}

# Where the kernel & boot modules will be
MODDIR=${DESTDIR}/multiboot

#
# Directory where to store temporary file system images
#
: ${IMG_DIR=${OBJ}/img}

CDFILES=${IMG_DIR}/cd

if [ ! -f ${BUILDSH} ]
then	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:$PATH

while getopts "i" c
do
	case "$c" in
		i)	: ${IMG=minix_x86.iso}
			ISOMODE=1
			;;
	esac
done

: ${IMG=minix_x86.img}

#
# Artifacts from this script are stored in the IMG_DIR
#
rm -rf $IMG_DIR $IMG
mkdir -p $IMG_DIR $CDFILES

#
# Call build.sh using a sloppy file list so we don't need to remove the installed /etc/fstag
#
export CPPFLAGS=${FLAG}
sh ${BUILDSH} -V SLOPPY_FLIST=yes -V MKBINUTILS=yes -V MKGCCCMDS=yes -j ${JOBS} -m ${ARCH} -O ${OBJ} -D ${DESTDIR} ${BUILDVARS} -U -u distribution

if [ "$ISOMODE" ]
then	cp ${DESTDIR}/usr/mdec/boot_monitor $CDFILES/boot
	cp ${MODDIR}/* $CDFILES/
	. ./releasetools/release.functions
	cd_root_changes	# uses $CDFILES and writes $CDFILES/boot.cfg
	# start the image off with the iso image; reduce root size to reserve
	${CROSS_TOOLS}/nbwriteisofs -s0x0 -l MINIX -B ${DESTDIR}/usr/mdec/bootxx_cd9660 -n $CDFILES $IMG
	ISO_SIZE=$((`stat -c %s $IMG` / 512))
else	# just make an empty iso partition
	ISO_SIZE=8
fi

#
# create a fstab entry in /etc this is normally done during the
# setup phase on x86
#
cat >${FSTAB} <<END_FSTAB
/dev/c0d0p2   /usr    mfs     rw                      0       2
/dev/c0d0p3   /home   mfs     rw                      0       2
END_FSTAB

rm -f ${DESTDIR}/SETS.*

${CROSS_TOOLS}/nbpwd_mkdb -V 0 -p -d ${DESTDIR} ${DESTDIR}/etc/master.passwd

# make the different file system. this part is *also* hacky. We first convert
# the METALOG.sanitised using mtree into a input METALOG containing uids and
# gids.
# After that we do some magic processing to add device nodes (also missing from METALOG)
# and convert the METALOG into a proto file that can be used by mkfs.mfs
#
echo "creating the file systems"

#
# read METALOG and use mtree to convert the user and group names into uid and gids
# FIX put "input somewhere clean"
#
cat ${DESTDIR}/METALOG.sanitised | ${CROSS_TOOLS}/nbmtree -N ${DESTDIR}/etc -C > ${IMG_DIR}/input

# add fstab
echo "./etc/fstab type=file uid=0 gid=0 mode=0755 size=747 time=1365060731.000000000" >> ${IMG_DIR}/input

# fill root.img (skipping /usr entries while keeping the /usr directory)
cat ${IMG_DIR}/input  | grep -v "^./usr/" | ${CROSS_TOOLS}/nbtoproto -b ${DESTDIR} -o ${IMG_DIR}/root.in

#
# add device nodes somewhere in the middle of the proto file. Better would be to add the entries in the
# original METALOG
# grab the first part
grep -B 10000 "^ dev"  ${IMG_DIR}/root.in >  ${IMG_DIR}/root.proto
# add the device nodes from the ramdisk
cat  ${OBJ}/drivers/ramdisk/proto.dev >> ${IMG_DIR}/root.proto
# and add the rest of the file
grep -A 10000 "^ dev"  ${IMG_DIR}/root.in | tail -n +2    >>  ${IMG_DIR}/root.proto
rm ${IMG_DIR}/root.in

#
# Create proto files for /usr and /home using toproto.
#
cat ${IMG_DIR}/input  | grep  "^\./usr/\|^. "  | sed "s,\./usr,\.,g" | ${CROSS_TOOLS}/nbtoproto -b ${DESTDIR}/usr -o ${IMG_DIR}/usr.proto
cat ${IMG_DIR}/input  | grep  "^\./home/\|^. "  | sed "s,\./home,\.,g" | ${CROSS_TOOLS}/nbtoproto -b ${DESTDIR}/home -o ${IMG_DIR}/home.proto

# This script creates a bootable image and should at some point in the future
# be replaced by makefs.
#
# All sized are written in 512 byte blocks
#
# we create a disk image of about 2 gig's
# for alignment reasons, prefer sizes which are multiples of 4096 bytes
#
: ${ROOT_SIZE=$((  64*(2**20) / 512))}
: ${HOME_SIZE=$(( 128*(2**20) / 512))}
: ${USR_SIZE=$(( 1536*(2**20) / 512))}

if [ "$ISOMODE" ]
then	
	# In iso mode, make all FSes fit (i.e. as small as possible), but
	# leave some space on /
	ROOTSIZEARG="-x 5"
else	
	# In hd image mode, FSes have fixed sizes
	ROOTSIZEARG="-b $((${ROOT_SIZE} / 8))"
	USRSIZEARG="-b $((${USR_SIZE} / 8))"
	HOMESIZEARG="-b $((${HOME_SIZE} / 8))"
fi

echo "Writing Minix filesystem images"

# Do some math to determine the start addresses of the partitions.
# Ensure the start of the partitions are always aligned, the end will 
# always be as we assume the sizes are multiples of 4096 bytes, which
# is always true as soon as you have an integer multiple of 1MB.
ROOT_START=$ISO_SIZE

echo " - ROOT"
ROOT_SIZE=$((`${CROSS_TOOLS}/nbmkfs.mfs -d $ROOTSIZEARG -I $(($ROOT_START*512)) $IMG ${IMG_DIR}/root.proto`/512))
USR_START=$(($ROOT_START + $ROOT_SIZE))
echo " - USR"
USR_SIZE=$((`${CROSS_TOOLS}/nbmkfs.mfs -d $USRSIZEARG  -I $(($USR_START*512))  $IMG ${IMG_DIR}/usr.proto`/512))
HOME_START=$(($USR_START + $USR_SIZE))
echo " - HOME"
HOME_SIZE=$((`${CROSS_TOOLS}/nbmkfs.mfs -d $HOMESIZEARG -I $(($HOME_START*512)) $IMG ${IMG_DIR}/home.proto`/512))

${CROSS_TOOLS}/nbpartition -m ${IMG} 0 81:${ISO_SIZE} 81:${ROOT_SIZE} 81:${USR_SIZE} 81:${HOME_SIZE} 

mods="`( cd $MODDIR; echo mod* | tr ' ' ',' )`"
if [ "$ISOMODE" ]
then	echo "CD image at `pwd`/$IMG"
else	echo "To boot this image on kvm:"
	echo "cd $MODDIR && kvm -serial stdio -kernel kernel -append \"console=tty00 rootdevname=c0d0p1\" -initrd \"$mods\" -hda `pwd`/$IMG"
fi
