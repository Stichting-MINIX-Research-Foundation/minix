#!/usr/bin/env bash
#
# Build an x86_64 UEFI-bootable MINIX installation CD image.
#
# This intentionally uses the EFI bootloader and Multiboot2 path.  The
# legacy x86_cdimage.sh script is for 32-bit BIOS boot and must not be used
# for the amd64 port.
#
set -e

# Resolve the source root so this helper also works when called by build.sh
# from a machine-dependent distrib/ subdirectory.
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
cd "${SCRIPT_DIR}/.."

: ${ARCH:=amd64}
: ${OBJ:=../build}
: ${TOOLCHAIN_TRIPLET:=x86_64-elf64-minix-}
: ${BUILDSH:=build.sh}
: ${SETS:=minix-base}
: ${BUNDLE_SETS:=1}
: ${CREATE_IMAGE_ONLY:=1}
: ${EFIMB:=64}
: ${RELEASEDIR:=${OBJ}/releasedir}
: ${SETS_DIR:=${RELEASEDIR}/${ARCH}/binary/sets}
: ${XORRISO:=xorriso}

if [ "${ARCH}" != "amd64" ]; then
	echo "This script creates an amd64 UEFI image; ARCH must be amd64." >&2
	exit 2
fi

if [ ! -f "${BUILDSH}" ]; then
	echo "Run this script from the source root (where ${BUILDSH} is)." >&2
	exit 1
fi

if ! command -v "${XORRISO}" >/dev/null 2>&1; then
	echo "xorriso is required to create an EFI El Torito ISO." >&2
	echo "Install it with: sudo apt install xorriso" >&2
	exit 1
fi

case "${EFIMB}" in
	''|*[!0-9]*)
		echo "EFIMB must be a positive integer (MiB)." >&2
		exit 2
		;;
esac
if [ "${EFIMB}" -eq 0 ]; then
	echo "EFIMB must be greater than zero." >&2
	exit 2
fi

# image.defaults supplies the common object/tool paths.  RELEASEDIR is the
# release root (not the old x86_cdimage.sh binary subdirectory).
. releasetools/image.defaults
. releasetools/image.functions

: ${IMG:=${RELEASEDIR}/images/Minix-${RELEASE_VERSION}-x86_64-install.iso}
ISO_VOLID=${ISO_VOLID:-$(printf 'MINIX_%s' "${RELEASE_VERSION}" | sed 's/[^A-Za-z0-9_]/_/g' | tr '[:lower:]' '[:upper:]')}

if [ ! -d "${DESTDIR}" ]; then
	echo "Missing DESTDIR: ${DESTDIR}" >&2
	echo "Build the amd64 release first, or set CREATE_IMAGE_ONLY=0." >&2
	exit 1
fi
if [ ! -d "${SETS_DIR}" ]; then
	echo "Missing release sets directory: ${SETS_DIR}" >&2
	exit 1
fi
if [ ! -s "${SETS_DIR}/minix-kernel.tgz" ]; then
	echo "Missing ${SETS_DIR}/minix-kernel.tgz; build the amd64 release first." >&2
	exit 1
fi

echo "Preparing amd64 UEFI installation media..."
build_workdir "${SETS}"
workdir_add_cd_files
workdir_add_kernel minix_default

# The EFI boot manager loads BOOTX64.EFI from a FAT EFI System Partition
# embedded as the EFI El Torito boot image.  The actual MINIX files stay on
# the ISO9660 filesystem and are read by efiboot's cd9660 support.
if [ ! -s "${ROOT_DIR}/usr/mdec/bootx64.efi" ]; then
	if [ ! -s "${DESTDIR}/usr/mdec/bootx64.efi" ]; then
		echo "Missing amd64 EFI loader: usr/mdec/bootx64.efi" >&2
		exit 1
	fi
	mkdir -p "${ROOT_DIR}/usr/mdec"
	cp "${DESTDIR}/usr/mdec/bootx64.efi" "${ROOT_DIR}/usr/mdec/bootx64.efi"
fi

cat > "${ROOT_DIR}/boot.cfg" <<'EOF'
banner=Welcome to MINIX 3 amd64 UEFI installation media
banner================================================================================
banner=
menu=Start MINIX 3 (UEFI x86_64):modules on; load_mods /boot/minix_default/mod*; multiboot2 /boot/minix_default/kernel bootcd=1 cdproberoot=1 acpi=yes ahci=yes cttyline=0 no_smp=0 no_apic=0
menu=Start MINIX 3 in single-user mode:modules on; load_mods /boot/minix_default/mod*; multiboot2 /boot/minix_default/kernel bootcd=1 cdproberoot=1 bootopts=-s acpi=yes ahci=yes
menu=Start MINIX 3 on serial console:modules on; load_mods /boot/minix_default/mod*; multiboot2 /boot/minix_default/kernel bootcd=1 cdproberoot=1 acpi=yes ahci=yes ttybaud=115200 console=tty00 consdev=com0
menu=Drop to UEFI boot prompt:prompt
clear=1
timeout=10
default=1
EOF

# Construct the FAT image consumed by UEFI's EFI El Torito entry.
EFI_DIR="${WORK_DIR}/efi"
EFI_IMAGE="${ROOT_DIR}/efiboot.img"
mkdir -p "${EFI_DIR}/EFI/BOOT"
cp "${ROOT_DIR}/usr/mdec/bootx64.efi" "${EFI_DIR}/EFI/BOOT/BOOTX64.EFI"
"${CROSS_TOOLS}/nbmakefs" -M "${EFIMB}m" -m "${EFIMB}m" \
	-t msdos -o F=32,c=1 "${EFI_IMAGE}" "${EFI_DIR}"

mkdir -p "$(dirname "${IMG}")"
rm -f "${IMG}"
echo "Writing UEFI El Torito ISO: ${IMG}"
"${XORRISO}" -as mkisofs \
	-iso-level 3 -r -J -joliet-long \
	-V "${ISO_VOLID}" -publisher "MINIX Project" \
	-e efiboot.img -no-emul-boot \
	-o "${IMG}" "${ROOT_DIR}"

if [ ! -s "${IMG}" ]; then
	echo "ISO creation failed: ${IMG} was not produced." >&2
	exit 1
fi

echo
echo "UEFI x86_64 ISO created: ${IMG}"
echo "Boot mode: UEFI x86_64 (EFI El Torito); legacy BIOS boot is not included."
echo "Release sets are available under /usr/amd64/binary/sets on the ISO."
