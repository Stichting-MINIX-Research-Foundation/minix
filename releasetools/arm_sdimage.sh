#!/bin/bash
set -e

if [ `id -u` -ne 0 ]
then	echo "This script should be running as root calling sudo"
	sudo $0
	exit 1
fi

: ${ARCH=evbearm-el}
: ${OBJ=../obj.${ARCH}}
: ${CROSS_TOOLS=${OBJ}/"tooldir.`uname -s`-`uname -r`-`uname -m`"/bin}
: ${CROSS_PREFIX=${CROSS_TOOLS}/arm-elf32-minix-}
: ${JOBS=-j4}
: ${DESTDIR=${OBJ}/destdir.$ARCH}
: ${FSTAB=${DESTDIR}/etc/fstab}

: ${MP_MINIX=/tmp/minix}
: ${MP_BOOT=/tmp/minixboot}

: ${LOOP=/dev/loop0}
: ${EMPTYIMG=minix_arm_sd_empty.img}
: ${IMG=minix_arm_sd.img}

BUILDSH=build.sh

if [ ! -f ${BUILDSH} ]
then	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

if [ ! -f ${EMPTYIMG}.bz2 ]
then	echo Retrieving ${EMPTYIMG}
	wget http://www.minix3.org/arm/${EMPTYIMG}.bz2
fi

if [ ! -f $IMG ]
then	echo decompressing ${EMPTYIMG} onto ${IMG}
	bzip2 -d -k ${EMPTYIMG}.bz2
	mv ${EMPTYIMG} ${IMG}
fi

# remove fstab and generated pw db
rm -rf ${DESTDIR}/etc

sh build.sh ${JOBS} -m ${ARCH} -O ${OBJ} -D ${DESTDIR} -u distribution 

cat >${FSTAB} <<END_FSTAB
/dev/c0d0p1s0   /       mfs     rw                      0       1
/dev/c0d0p1s2   /usr    mfs     rw                      0       2
/dev/c0d0p1s1   /home   mfs     rw                      0       2
END_FSTAB

rm -f ${DESTDIR}/SETS.*

${CROSS_TOOLS}/nbpwd_mkdb -V 0 -p -d ${DESTDIR} ${DESTDIR}/etc/master.passwd

umount ${MP_MINIX}/home || true
umount ${MP_MINIX}/usr || true
umount ${MP_MINIX} || true
umount ${MP_BOOT} || true

losetup -d ${LOOP} || true

echo "Mounting disk image."
losetup ${LOOP} $IMG

${CROSS_TOOLS}/nbmkfs.mfs ${LOOP}p5
${CROSS_TOOLS}/nbmkfs.mfs ${LOOP}p6
${CROSS_TOOLS}/nbmkfs.mfs ${LOOP}p7

mkdir -p ${MP_BOOT}
mount ${LOOP}p1 ${MP_BOOT}

mkdir -p ${MP_MINIX}
mount ${LOOP}p5 ${MP_MINIX}

mkdir -p ${MP_MINIX}/home
mkdir -p ${MP_MINIX}/usr
mount ${LOOP}p6 ${MP_MINIX}/home
mount ${LOOP}p7 ${MP_MINIX}/usr

echo "Synchronizing filesystem with newly built binaries."
rsync -a ${DESTDIR}/ ${MP_MINIX}/

echo "Copying configuration and boot modules."
cp releasetools/uEnv.txt releasetools/cmdline.txt ${MP_BOOT}

# also copy to the obj directory to allow tftp booting
# using obj dir as root
cp releasetools/uEnv.txt releasetools/cmdline.txt ${OBJ}

${CROSS_PREFIX}objcopy ${OBJ}/kernel/kernel -O binary ${OBJ}/kernel.bin
cp ${OBJ}/kernel.bin ${MP_BOOT}

for f in vm rs pm sched vfs ds mfs pfs init
do
    cp ${OBJ}/servers/${f}/${f} ${OBJ}/${f}.elf
    ${CROSS_PREFIX}strip -s ${OBJ}/${f}.elf
    cp ${OBJ}/${f}.elf ${MP_BOOT}
done

for f in tty memory log
do
    cp ${OBJ}/drivers/${f}/${f} ${OBJ}/${f}.elf
    ${CROSS_PREFIX}strip -s ${OBJ}/${f}.elf
    cp ${OBJ}/${f}.elf ${MP_BOOT}
done

# Unmount disk image
echo "Unmounting disk image"
sync

umount ${MP_MINIX}/home
umount ${MP_MINIX}/usr
umount ${MP_MINIX}
umount ${MP_BOOT}
losetup -d ${LOOP}
