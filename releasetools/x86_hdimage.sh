#!/usr/bin/env bash
set -e

#
# This script creates a bootable image and should at some point in the future
# be replaced by makefs.
#

: ${ARCH=i386}
: ${OBJ=../obj.${ARCH}}
: ${CROSS_TOOLS=${OBJ}/"tooldir.`uname -s`-`uname -r`-`uname -m`"/bin}
: ${CROSS_PREFIX=${CROSS_TOOLS}/i586-elf32-minix-}
: ${JOBS=1}
: ${DESTDIR=${OBJ}/destdir.$ARCH}
: ${RELEASETOOLSDIR=./releasetools/}
: ${FSTAB=${DESTDIR}/etc/fstab}
: ${BUILDVARS=}
: ${BUILDSH=build.sh}
: ${CREATE_IMAGE_ONLY=0}
: ${RC=minix_x86.rc}

#
# Directory where to store temporary file system images
#
: ${IMG_DIR=${OBJ}/img}
: ${CDFILES=${IMG_DIR}/cd}

# All sized are written in 512 byte blocks
#
# we create a disk image of about 2 gig's
# for alignment reasons, prefer sizes which are multiples of 4096 bytes
#
: ${ROOT_SIZE=$((   64*(2**20) / 512))}
: ${HOME_SIZE=$((  128*(2**20) / 512))}
: ${USR_SIZE=$((  1792*(2**20) / 512))}

#
# Do some math to determine the start addresses of the partitions.
# Don't leave holes so the 'partition' invocation later is easy.
#


# Where the kernel & boot modules will be
MODDIR=${DESTDIR}/boot/minix/.temp

while getopts "i" c
do
	case "$c" in
		i)	: ${IMG=minix_x86.iso}
			ISOMODE=1
			;;
	esac
done

: ${IMG=minix_x86.img}

if [ "x${ISOMODE}" = "x1" ]
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

if [ ! -f ${BUILDSH} ]
then
	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:${PATH}

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
rm -rf ${IMG_DIR} ${IMG}
mkdir -p ${IMG_DIR} ${CDFILES}

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

if [ "x${ISOMODE}" = "x1" ]
then
	cp ${DESTDIR}/usr/mdec/boot_monitor ${CDFILES}/boot
	cp ${MODDIR}/* ${CDFILES}/
	. ${RELEASETOOLSDIR}/release.functions
	cd_root_changes	# uses $CDFILES and writes $CDFILES/boot.cfg
	# start the image off with the iso image; reduce root size to reserve
	${CROSS_TOOLS}/nbwriteisofs -s0x0 -l MINIX -B ${DESTDIR}/usr/mdec/bootxx_cd9660 -n ${CDFILES} ${IMG}
	ISO_SIZE=$((`${CROSS_TOOLS}/nbstat -f %z ${IMG}` / 512))
else
	# just make an empty iso partition
	ISO_SIZE=8
fi

#
# Generate /root, /usr and /home partition images.
#
echo "Writing Minix filesystem images"
ROOT_START=${ISO_SIZE}
echo " - ROOT"
_ROOT_SIZE=$((`${CROSS_TOOLS}/nbmkfs.mfs -d ${ROOTSIZEARG} -I $((${ROOT_START}*512)) ${IMG} ${IMG_DIR}/root.proto`/512))
USR_START=$((${ROOT_START} + ${_ROOT_SIZE}))
echo " - USR"
_USR_SIZE=$((`${CROSS_TOOLS}/nbmkfs.mfs  -d ${USRSIZEARG}  -I $((${USR_START}*512))  ${IMG}  ${IMG_DIR}/usr.proto`/512))
HOME_START=$((${USR_START} + ${_USR_SIZE}))
echo " - HOME"
_HOME_SIZE=$((`${CROSS_TOOLS}/nbmkfs.mfs -d ${HOMESIZEARG} -I $((${HOME_START}*512)) ${IMG} ${IMG_DIR}/home.proto`/512))

#
# Write the partition table using the natively compiled
# minix partition utility
#
${CROSS_TOOLS}/nbpartition -m ${IMG} 0 81:${ISO_SIZE} \
	81:${_ROOT_SIZE} 81:${_USR_SIZE} 81:${_HOME_SIZE}

mods="`( cd ${MODDIR}; echo mod* | tr ' ' ',' )`"
if [ "x${ISOMODE}" = "x1" ]
then
	echo "CD image at `pwd`/${IMG}"
else
	echo "To boot this image on kvm:"
	echo "cd ${MODDIR} && qemu-system-i386 -display none -serial stdio -kernel kernel -append \"console=tty00 rootdevname=c0d0p1\" -initrd \"${mods}\" -hda `pwd`/${IMG} --enable-kvm"
fi
