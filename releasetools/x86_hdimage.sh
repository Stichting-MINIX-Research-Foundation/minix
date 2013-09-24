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

#
# Directory where to store temporary file system images
#
: ${IMG_DIR=${OBJ}/img}
: ${IMG=minix_x86.img}

if [ ! -f ${BUILDSH} ]
then	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:$PATH

#
# Artifacts from this script are stored in the IMG_DIR
#
mkdir -p $IMG_DIR

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
# for alignement reasons, prefer sizes which are multiples of 4096 bytes
#
: ${IMG_SIZE=$((   2*(2**30) / 512))}
: ${ROOT_SIZE=$((  64*(2**20) / 512))}
: ${HOME_SIZE=$(( 128*(2**20) / 512))}
: ${USR_SIZE=$((1536*(2**20) / 512))}

#
# create a fstab entry in /etc this is normally done during the
# setup phase on x86
#
cat >${FSTAB} <<END_FSTAB
/dev/c0d0p1   /home   mfs     rw                      0       2
/dev/c0d0p2   /usr    mfs     rw                      0       2
END_FSTAB

rm -f ${DESTDIR}/SETS.*

${CROSS_TOOLS}/nbpwd_mkdb -V 0 -p -d ${DESTDIR} ${DESTDIR}/etc/master.passwd

#
# Now given the sizes above use DD to create separate files representing
# the partitions we are going to use.
#
dd if=/dev/zero of=${IMG_DIR}/root.img bs=512 count=1 seek=$(($ROOT_SIZE -1)) 2>/dev/null
dd if=/dev/zero of=${IMG_DIR}/home.img bs=512 count=1 seek=$(($HOME_SIZE -1)) 2>/dev/null
dd if=/dev/zero of=${IMG_DIR}/usr.img bs=512 count=1 seek=$(($USR_SIZE -1)) 2>/dev/null

# Create the empty image where we later will but the partitions in
dd if=/dev/zero of=${IMG} bs=512 count=1 seek=$(($IMG_SIZE -1))

#
# Do some math to determine the start addresses of the partitions.
# Ensure the start of the partitions are always aligned, the end will 
# always be as we assume the sizes are multiples of 4096 bytes, which
# is always true as soon as you have an integer multiple of 1MB.
#
ROOT_START=8
HOME_START=$(($ROOT_START + $ROOT_SIZE))
USR_START=$(($HOME_START + $HOME_SIZE))

set -x
${CROSS_TOOLS}/nbpartition -m ${IMG} ${ROOT_START} 81:${ROOT_SIZE} 81:${HOME_SIZE} 81:${USR_SIZE}
set +x

# make the different file system. this part is *also* hacky. We first convert
# the METALOG.sanitised using mtree into a input METALOG containing uids and
# gids.
# Afther that we do some magic processing to add device nodes (also missing from METALOG)
# and convert the METALOG into a proto file that can be used by mkfs.mfs
#
echo "creating the file systems"

#
# read METALOG and use mtree to conver the user and group names into uid and gids
# FIX put "input somwhere clean"
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

#
# Generate /root, /usr and /home partition images.
#
echo "Writing Minix filesystem images"
echo " - ROOT"
${CROSS_TOOLS}/nbmkfs.mfs -b $((${ROOT_SIZE} / 8)) ${IMG_DIR}/root.img ${IMG_DIR}/root.proto
echo " - USR"
${CROSS_TOOLS}/nbmkfs.mfs -b $((${USR_SIZE} / 8))  ${IMG_DIR}/usr.img  ${IMG_DIR}/usr.proto
echo " - HOME"
${CROSS_TOOLS}/nbmkfs.mfs -b $((${HOME_SIZE} / 8)) ${IMG_DIR}/home.img ${IMG_DIR}/home.proto

#
# Merge the partitions into a single image.
#
echo "Merging file systems"
dd if=${IMG_DIR}/root.img of=${IMG} seek=$ROOT_START conv=notrunc
dd if=${IMG_DIR}/home.img of=${IMG} seek=$HOME_START conv=notrunc
dd if=${IMG_DIR}/usr.img of=${IMG} seek=$USR_START conv=notrunc

moddir=${DESTDIR}/boot/minix/.temp/
mods="`( cd $moddir; echo mod* | tr ' ' ',' )`"
echo "To boot this image on kvm:"
echo "cd $moddir && kvm -serial stdio -kernel kernel -append \"console=tty00 rootdevname=c0d0p0\" -initrd \"$mods\" -hda `pwd`/$IMG"
