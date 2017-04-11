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

: ${IMG=minix_pkgsrc.iso}
: ${SETS=}
: ${CREATE_IMAGE_ONLY=1}

if [ ! -f ${BUILDSH} ]
then
	echo "Please invoke me from the root source dir, where ${BUILDSH} is."
	exit 1
fi

# set up disk creation environment
. releasetools/image.defaults
. releasetools/image.functions

echo "Building work directory..."
build_workdir "$SETS"

echo "Bundling packages..."
bundle_packages "$BUNDLE_PACKAGES"

echo "Creating specification files..."
cat > ${WORK_DIR}/extra.base <<EOF
. type=dir uid=0 gid=0 mode=0755
./usr type=dir uid=0 gid=0 mode=0755
EOF
create_input_spec
create_protos

# Clean image
if [ -f ${IMG} ]	# IMG might be a block device
then
	rm -f ${IMG}
fi

echo "Writing ISO..."
${CROSS_TOOLS}/nbmakefs -t cd9660 -F ${WORK_DIR}/input -o "rockridge,label=MINIX_PKGSRC" ${IMG} ${ROOT_DIR}

echo ""
echo "ISO image at `pwd`/${IMG}"
