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
# Rebuild release sets by default. Image-only mode consumes the existing
# minix-base.tgz, which can silently omit source changes to /etc/rc,
# /etc/ttys, and other installed files even when the ISO is recreated.
# Set CREATE_IMAGE_ONLY=1 only when the release sets already match this tree.
: ${CREATE_IMAGE_ONLY:=0}
: ${EFIMB:=64}
: ${RELEASEDIR:=${OBJ}/releasedir}
: ${SETS_DIR:=${RELEASEDIR}/${ARCH}/binary/sets}

if [ "${ARCH}" != "amd64" ]; then
	echo "This script creates an amd64 UEFI image; ARCH must be amd64." >&2
	exit 2
fi

if [ ! -f "${BUILDSH}" ]; then
	echo "Run this script from the source root (where ${BUILDSH} is)." >&2
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

# The installer root is extracted from release-set archives.  On an
# incremental build those archives can lag behind the checked-out source,
# even when the ISO itself is recreated.  Keep the two files that control
# rc logging and the active console getty in sync with this source tree.
if ! grep -Fq 'if [ "$RC_LOG_FILE" = "/dev/log" ]; then' etc/rc; then
	echo "etc/rc is missing the /dev/log logging fallback; refusing to create a stale installer image." >&2
	exit 1
fi
cp -p etc/rc etc/ttys "${ROOT_DIR}/etc/"
if ! grep -Fq 'RC_LOG_FILE="/var/run/rc.log"' "${ROOT_DIR}/etc/rc"; then
	echo "The staged installer /etc/rc does not contain the rc log fallback." >&2
	exit 1
fi

# Do not call workdir_add_cd_files() here: that legacy helper copies
# usr/mdec/boot_monitor and creates /minixboot.  The amd64 port boots through
# BOOTX64.EFI and Multiboot2, and does not build the BIOS boot monitor.
mkdir -p "${ROOT_DIR}/etc"
cat > "${ROOT_DIR}/etc/fstab" <<'EOF_FSTAB'
none            /sys            devman  rw,rslabel=devman       0       0
none            /dev/pts        ptyfs   rw,rslabel=ptyfs         0       0
EOF_FSTAB
add_file_spec "etc/fstab" extra.fstab

# Use amd64 UEFI-specific notes instead of the inherited i386/IDE-era text.
cp releasetools/release/cd/README.amd64.UEFI.TXT "${ROOT_DIR}/README.TXT"
add_file_spec "README.TXT" extra.cdfiles

# The ISO9660 root is read-only after boot.  Arrange for the logical
# MINIX console (character device 4:0) to be created by nbmakefs from an
# mtree entry; unlike host mknod, this also works for unprivileged builds.
mkdir -p "${ROOT_DIR}/dev"
if [ -d "${ROOT_DIR}/dev/console" ]; then
	echo "${ROOT_DIR}/dev/console exists as a directory, expected a device node." >&2
	exit 1
fi
rm -f "${ROOT_DIR}/dev/console"
CONSOLE_SPEC="${WORK_DIR}/uefi-console.mtree"
cat > "${CONSOLE_SPEC}" <<'EOF_CONSOLE'
. type=dir uid=0 gid=0 mode=0755
./dev type=dir uid=0 gid=0 mode=0755
./dev/console type=char device=netbsd,4,0 mode=600 uid=0 gid=0
EOF_CONSOLE

workdir_add_kernel minix_default

# The MINIX EFI boot monitor's block-device interface exposes the optical
# ISO9660 volume as hdN a (partition index zero).  In an El Torito boot the
# firmware may not identify that volume as the bootloader's default device,
# so unqualified /boot paths can resolve against no device at all.  Keep the
# default suitable for a VM with only the installer CD attached, while
# allowing builders to select another hdN a when a firmware enumerates disks
# before the CD-ROM.
: ${UEFI_CD_DEVICE:=hd0a}
if [[ ! "${UEFI_CD_DEVICE}" =~ ^hd[0-9]+[a-z]$ ]]; then
	echo "UEFI_CD_DEVICE must be an efiboot device such as hd0a (got: ${UEFI_CD_DEVICE})." >&2
	exit 2
fi

BOOT_MODULES="load ${UEFI_CD_DEVICE}:/boot/minix_default/mod01_ds; load ${UEFI_CD_DEVICE}:/boot/minix_default/mod02_rs; load ${UEFI_CD_DEVICE}:/boot/minix_default/mod03_pm; load ${UEFI_CD_DEVICE}:/boot/minix_default/mod04_sched; load ${UEFI_CD_DEVICE}:/boot/minix_default/mod05_vfs; load ${UEFI_CD_DEVICE}:/boot/minix_default/mod06_memory; load ${UEFI_CD_DEVICE}:/boot/minix_default/mod07_tty; load ${UEFI_CD_DEVICE}:/boot/minix_default/mod08_mib; load ${UEFI_CD_DEVICE}:/boot/minix_default/mod09_vm; load ${UEFI_CD_DEVICE}:/boot/minix_default/mod10_pfs; load ${UEFI_CD_DEVICE}:/boot/minix_default/mod11_mfs; load ${UEFI_CD_DEVICE}:/boot/minix_default/mod12_init"

# Fail the ISO build instead of generating media with an incomplete boot tree.
for file in kernel mod01_ds mod02_rs mod03_pm mod04_sched mod05_vfs mod06_memory mod07_tty mod08_mib mod09_vm mod10_pfs mod11_mfs mod12_init; do
	if [ ! -s "${ROOT_DIR}/boot/minix_default/${file}" ]; then
		echo "Missing UEFI boot file: boot/minix_default/${file}" >&2
		exit 1
	fi
done

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

cat > "${ROOT_DIR}/boot.cfg" <<EOF
banner=Welcome to MINIX 3 amd64 UEFI installation media
banner================================================================================
banner=
menu=Start MINIX 3 (UEFI x86_64):modules on; ${BOOT_MODULES}; multiboot2 ${UEFI_CD_DEVICE}:/boot/minix_default/kernel bootcd=1 cdproberoot=1 acpi=yes
menu=Start MINIX 3 in single-user mode:modules on; ${BOOT_MODULES}; multiboot2 ${UEFI_CD_DEVICE}:/boot/minix_default/kernel bootcd=1 cdproberoot=1 bootopts=-s
menu=Start MINIX 3 on serial console:consdev com0,115200; modules on; ${BOOT_MODULES}; multiboot2 ${UEFI_CD_DEVICE}:/boot/minix_default/kernel bootcd=1 cdproberoot=1 acpi=yes console=tty00
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
IMG_DIR=$(cd "$(dirname "${IMG}")" && pwd)
IMG_PATH="${IMG_DIR}/$(basename "${IMG}")"
NBMAKEFS="$(cd "${CROSS_TOOLS}" && pwd)/nbmakefs"
rm -f "${IMG_PATH}"
echo "Writing UEFI El Torito ISO with nbmakefs: ${IMG_PATH}"
(
	cd "${ROOT_DIR}"
	"${NBMAKEFS}" -N "${ROOT_DIR}/etc" -t cd9660 \
		-F "${CONSOLE_SPEC}" \
		-o "rockridge,isolevel=2,allow-multidot,label=${ISO_VOLID},publisher=MINIX Project,bootimage=efi;efiboot.img,no-emul-boot" \
		"${IMG_PATH}" .
)

if [ ! -s "${IMG_PATH}" ]; then
	echo "ISO creation failed: ${IMG_PATH} was not produced." >&2
	exit 1
fi

echo
echo "UEFI x86_64 ISO created: ${IMG}"
echo "Boot mode: UEFI x86_64 (EFI El Torito); legacy BIOS boot is not included."
echo "Release sets are available under /usr/amd64/binary/sets on the ISO."
