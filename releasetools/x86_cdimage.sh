#!/usr/bin/env bash
set -e

#
# This script creates a bootable CD-ROM image.
#

: ${ARCH=i386}
: ${OBJ=../obj.${ARCH}}
: ${CROSS_TOOLS=${OBJ}/"tooldir.`uname -s`-`uname -r`-`uname -m`"/bin}
: ${CROSS_PREFIX=${CROSS_TOOLS}/i586-elf32-minix-}
: ${JOBS=1}
: ${DESTDIR=${OBJ}/destdir.$ARCH}
: ${RELEASEDIR=${OBJ}/releasedir/$ARCH/binary}
: ${RELEASETOOLSDIR=./releasetools/}
: ${FSTAB=${DESTDIR}/etc/fstab}
: ${BUILDVARS=}
: ${BUILDSH=build.sh}
: ${CREATE_IMAGE_ONLY=0}
: ${RC=minix_x86.rc}

: ${WORK_DIR=${OBJ}/work}
: ${SETS_DIR=${OBJ}/releasedir/${ARCH}/binary/sets}

# Add a directory to a spec file
#$1 : directory to add
#$2 : spec file
add_dir_spec() {
	echo "./$1 type=dir uid=0 gid=0 mode=0755" >> ${WORK_DIR}/$2
}

# Add a file to a spec file
#$1 : file to add
#$2 : spec file
add_file_spec() {
	echo "./$1 type=file uid=0 gid=0 mode=0755 size=$(wc -c < ${ROOT_DIR}/${1})" >> ${WORK_DIR}/$2
}

# Create the workdir (a directory where Minix is built using sets)
# spec files are put in WORK_DIR, the file system created in ROOT_DIR
#$1 : sets to extract
build_workdir() {
	# Extract sets
	mkdir ${ROOT_DIR}
	for set in $1; do
		if [ ! -e ${SETS_DIR}/${set}.tgz ]; then
			echo "Missing ${SETS_DIR}/${set}.tgz, aborting"
			echo "Are the release sets tarballs created?"
			exit 1
		fi
		(cd ${ROOT_DIR}; ${CROSS_TOOLS}/nbpax -rnz -f ${SETS_DIR}/${set}.tgz .)
	done

	# Build login/password files
	${CROSS_TOOLS}/nbpwd_mkdb -V 0 -p -d ${ROOT_DIR} ${ROOT_DIR}/etc/master.passwd

	# Build specifications files
	cp ${ROOT_DIR}/etc/mtree/set* ${WORK_DIR}
	${ROOT_DIR}/usr/bin/MAKEDEV -s -m all >> ${WORK_DIR}/extra.dev
}

# Add tarball sets to the workdir (for installation CD)
workdir_add_sets() {
	# Add sets to the root
	mkdir ${ROOT_DIR}/${ARCH}; add_dir_spec "${ARCH}" extra.sets
	mkdir ${ROOT_DIR}/${ARCH}/binary; add_dir_spec "${ARCH}/binary" extra.sets
	mkdir ${ROOT_DIR}/${ARCH}/binary/sets; add_dir_spec "${ARCH}/binary/sets" extra.sets

	DEST_SETS_DIR="${ARCH}/binary/sets"
	for set in ${SETS_DIR}/*.tgz; do
		# Copy set itself
		cp ${set} ${ROOT_DIR}/${DEST_SETS_DIR}
		add_file_spec "${DEST_SETS_DIR}/$(basename ${set})" extra.sets
		# Add file count
		COUNT_SRC=$(echo $(basename ${set}) | sed -e "s/\(.*\)\.tgz/\set.\1/")
		COUNT_NAME=$(echo $(basename ${set}) | sed -e "s/\.tgz/\.count/")
		wc -l < ${DESTDIR}/etc/mtree/${COUNT_SRC} >> ${ROOT_DIR}/${DEST_SETS_DIR}/${COUNT_NAME}
		add_file_spec "${DEST_SETS_DIR}/${COUNT_NAME}" extra.sets
	done

	# Add checksums
	cp ${SETS_DIR}/MD5 ${ROOT_DIR}/${DEST_SETS_DIR}
	add_file_spec "${DEST_SETS_DIR}/MD5" extra.sets
	cp ${SETS_DIR}/SHA512 ${ROOT_DIR}/${DEST_SETS_DIR}
	add_file_spec "${DEST_SETS_DIR}/SHA512" extra.sets
}

# Add CD boot files to the workdir
workdir_add_cdfiles() {
	cat >${ROOT_DIR}/boot.cfg <<END_BOOT_CFG
banner=Welcome to the MINIX 3 installation CD
banner================================================================================
banner=
menu=Regular MINIX 3:multiboot /boot/minix/.temp/kernel bootcd=1 cdproberoot=1 rootdevname=ram disable=inet
menu=Regular MINIX 3 (with AHCI):multiboot /boot/minix/.temp/kernel bootcd=1 cdproberoot=1 rootdevname=ram disable=inet ahci=yes
menu=Edit menu option:edit
menu=Drop to boot prompt:prompt
clear=1
timeout=10
default=1
load=/boot/minix/.temp/mod01_ds
load=/boot/minix/.temp/mod02_rs
load=/boot/minix/.temp/mod03_pm
load=/boot/minix/.temp/mod04_sched
load=/boot/minix/.temp/mod05_vfs
load=/boot/minix/.temp/mod06_memory
load=/boot/minix/.temp/mod07_tty
load=/boot/minix/.temp/mod08_mfs
load=/boot/minix/.temp/mod09_vm
load=/boot/minix/.temp/mod10_pfs
load=/boot/minix/.temp/mod11_init
END_BOOT_CFG
	for i in $(seq 18); do
		echo "# This space intentionally left blank - leave to appease bootloader!" >> ${ROOT_DIR}/boot.cfg
	done
	add_file_spec "boot.cfg" extra.cdfiles

	# Add boot monitor
	cp ${DESTDIR}/usr/mdec/boot_monitor ${ROOT_DIR}/minixboot
	add_file_spec "minixboot" extra.cdfiles
}

: ${IMG=minix_x86.iso}

if [ ! -f ${BUILDSH} ]
then
	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

#
# Are we going to build the minix sources?
#

if [ ${CREATE_IMAGE_ONLY} -eq 1 ]
then
	if [ ! -d ${DESTDIR} ]
	then
		echo "Minix source code doesn't appear to have been built."
		echo "Please try with \$CREATE_IMAGE_ONLY set to 0."
		exit 1
	fi
	if [ ! -d ${RELEASEDIR} ]
	then
		echo "Minix release tarball sets don't appear to have been created."
		echo "Please try with \$CREATE_IMAGE_ONLY set to 0."
		exit 1
	fi
fi

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
	sh ${BUILDSH} -j ${JOBS} -m ${ARCH} -O ${OBJ} -D ${DESTDIR} ${BUILDVARS} -U -u release

fi

# Sanity check
if [ -d "${WORK_DIR}/.git" ]; then
	echo "WORK_DIR directory has a Git repository in it, abort!"
	exit 1
fi

# Clean working directory
if [ -e "${WORK_DIR}" ]; then
	rm -rf "${WORK_DIR}"
fi
mkdir -p ${WORK_DIR}

# Get absolute paths to those directories
CROSS_TOOLS=$(cd ${CROSS_TOOLS} && pwd)
DESTDIR=$(cd ${DESTDIR} && pwd)
OBJ=$(cd ${OBJ} && pwd)
SETS_DIR=$(cd ${SETS_DIR} && pwd)
WORK_DIR=$(cd ${WORK_DIR} && pwd)
ROOT_DIR=${WORK_DIR}/fs

echo "Building work directory..."
build_workdir "minix-base"

echo "Adding extra files..."
workdir_add_sets
workdir_add_cdfiles

# create a fstab entry in /etc this is normally done during the
# setup phase on x86
cat >${ROOT_DIR}/etc/fstab <<END_FSTAB
none		/sys	devman	rw,rslabel=devman	0	0
END_FSTAB

# add fstab
add_file_spec "etc/fstab" extra.fstab

#
# read METALOG and use mtree to convert the user and group names into uid and gids
#
cat ${WORK_DIR}/set* ${WORK_DIR}/extra* | ${CROSS_TOOLS}/nbmtree -N ${ROOT_DIR}/etc -C -K device > ${WORK_DIR}/input

echo "Creating ISO..."
# create proto file (not used, for debugging purposes)
${CROSS_TOOLS}/nbtoproto < ${WORK_DIR}/input -b ${ROOT_DIR} -o ${WORK_DIR}/root.proto

# create the ISO
${CROSS_TOOLS}/nbmakefs -t cd9660 -F ${WORK_DIR}/input -o "rockridge,bootimage=i386;${DESTDIR}/usr/mdec/bootxx_cd9660,label=MINIX" ${IMG} ${ROOT_DIR}

echo "CD image at `pwd`/${IMG}"
echo "To boot this image on kvm:"
echo "qemu-system-i386 -display curses -cdrom `pwd`/${IMG} --enable-kvm"
