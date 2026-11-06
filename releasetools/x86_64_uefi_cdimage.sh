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


# Create pkgsrc.tgz before build_workdir() bundles all release sets on the ISO.
# The installer extracts every release-set archive into /usr, including this
# additional set rooted at usr/pkgsrc.
: ${PKGSRC_COMMIT:=18dfea989391bf165dfe51a1667f5df44ec94863}
: ${PKGSRC_ARCHIVE:=}
if [ -n "${PKGSRC_ARCHIVE}" ]; then
	python3 releasetools/pkgsrc_set.py \
		--sets-dir "${SETS_DIR}" --work-dir "${WORK_DIR}" \
		--archive "${PKGSRC_ARCHIVE}"
else
	python3 releasetools/pkgsrc_set.py \
		--sets-dir "${SETS_DIR}" --work-dir "${WORK_DIR}" \
		--commit "${PKGSRC_COMMIT}"
fi

echo "Preparing amd64 UEFI installation media..."
build_workdir "${SETS}"

# Keep the installer root's MAKEDEV in sync with this source tree. The
# minix-base release archive can be older than the ISO when image-only mode
# is used, which would otherwise retain broken device-name parsing.
mkdir -p "${ROOT_DIR}/dev"
# The release set already contains /dev/MAKEDEV, installed read-only.  Copying
# over that file directly fails for an unprivileged build user.  Stage the new
# script beside it, set its final mode, and rename it into place instead.
MAKEDEV_TMP="${ROOT_DIR}/dev/.MAKEDEV.tmp"
cp minix/commands/MAKEDEV/MAKEDEV.sh "${MAKEDEV_TMP}"
chmod 0755 "${MAKEDEV_TMP}"
mv -f "${MAKEDEV_TMP}" "${ROOT_DIR}/dev/MAKEDEV"

# The MINIX-specific release set manifest must include the bmake executable.
# Refuse to produce an ISO that would boot without the build tool.
if [ ! -x "${ROOT_DIR}/usr/bin/bmake" ]; then
	echo "Missing /usr/bin/bmake in the staged ${SETS} release set; rebuild minix-base.tgz." >&2
	exit 1
fi

# The installer root is extracted from release-set archives.  On an
# incremental build those archives can lag behind the checked-out source,
# even when the ISO itself is recreated.  Keep the boot and post-mount rc
# scripts, plus the active console table, synchronized with this source tree.
if ! grep -Fq 'if [ "$RC_LOG_FILE" = "/dev/log" ]; then' etc/rc; then
	echo "etc/rc is missing the /dev/log logging fallback; refusing to create a stale installer image." >&2
	exit 1
fi
cp -p etc/rc "${ROOT_DIR}/etc/rc"
cp -p etc/ttys "${ROOT_DIR}/etc/ttys"
# Autologin on the primary console and start the guided installer.
awk '
/^console[[:space:]]/ {
	print "console\t\"/usr/libexec/getty minix-installer\"\twsvt25\ton secure"
	next
}
{ print }
' "${ROOT_DIR}/etc/ttys" > "${WORK_DIR}/installer.ttys"
mv "${WORK_DIR}/installer.ttys" "${ROOT_DIR}/etc/ttys"
INSTALLER_DIR="${SCRIPT_DIR}/../distrib/amd64/installimage"
cp -p "${INSTALLER_DIR}/install.sh" "${ROOT_DIR}/install.sh"
cp -p "${INSTALLER_DIR}/setup-amd64.sh" "${ROOT_DIR}/setup-amd64.sh"
awk '
$0 == "rc_real_work \"$@\" 2>&1 | rc_postprocess" {
	print "# Autostart MINIX amd64 UEFI installer on the console."
	print "if [ -x /install.sh ]; then"
	print "\techo \"# MINIX amd64 installer\" > /etc/gettytab"
	print "\techo -n \"minix-installer:al=root:lo=/install.sh:im=:sp#\" >> /etc/gettytab"
	print "\tstty speed >> /etc/gettytab"
	print "fi"
}
{ print }
' "${ROOT_DIR}/etc/rc" > "${WORK_DIR}/installer.rc"
mv "${WORK_DIR}/installer.rc" "${ROOT_DIR}/etc/rc"
if ! grep -Fq 'Autostart MINIX amd64 UEFI installer' "${ROOT_DIR}/etc/rc"; then
	echo "Could not add the installer startup hook to the staged /etc/rc." >&2
	exit 1
fi
mkdir -p "${ROOT_DIR}/usr/etc"
cp -p etc/usr/rc "${ROOT_DIR}/usr/etc/rc"

# Include the interactive amd64 UEFI/GPT installer in the live ISO.
# workdir_add_sets() places the release archives under /usr/amd64/binary/sets.
mkdir -p "${ROOT_DIR}/usr/sbin"
cp -p distrib/amd64/installer/install.sh "${ROOT_DIR}/usr/sbin/minix-install"
chmod 0755 "${ROOT_DIR}/usr/sbin/minix-install"
# Carry the fixed MAKEDEV independently from minix-base.tgz. The installer
# extracts that archive onto the target, which would otherwise restore the
# older MAKEDEV before populating the target /dev directory.
cp -p minix/commands/MAKEDEV/MAKEDEV.sh "${ROOT_DIR}/minix-MAKEDEV"
chmod 0755 "${ROOT_DIR}/minix-MAKEDEV"

# Build a separate ESP image for the installed disk.  The El Torito image
# above intentionally has no boot.cfg because CD boot reads it from ISO9660.
# The installed system instead needs its boot menu on the ESP itself.
INSTALL_EFI_DIR="${WORK_DIR}/install-efi"
INSTALL_EFI_IMAGE="${ROOT_DIR}/install-efiboot.img"
mkdir -p "${INSTALL_EFI_DIR}/EFI/BOOT"
cp "${ROOT_DIR}/usr/mdec/bootx64.efi" "${INSTALL_EFI_DIR}/EFI/BOOT/BOOTX64.EFI"
cat > "${INSTALL_EFI_DIR}/boot.cfg" <<'EOF_INSTALL_BOOTCFG'
banner=Welcome to MINIX 3.4.0 amd64 (UEFI)
banner================================================================================
banner=
menu=Start MINIX 3 amd64:modules on; load hd0b:/boot/minix_default/mod01_ds; load hd0b:/boot/minix_default/mod02_rs; load hd0b:/boot/minix_default/mod03_pm; load hd0b:/boot/minix_default/mod04_sched; load hd0b:/boot/minix_default/mod05_vfs; load hd0b:/boot/minix_default/mod06_memory; load hd0b:/boot/minix_default/mod07_tty; load hd0b:/boot/minix_default/mod08_mib; load hd0b:/boot/minix_default/mod09_vm; load hd0b:/boot/minix_default/mod10_pfs; load hd0b:/boot/minix_default/mod11_mfs; load hd0b:/boot/minix_default/mod12_init; multiboot2 hd0b:/boot/minix_default/kernel rootdevname=c0d0p1 acpi=yes ahci=yes cttyline=0 no_smp=0 no_apic=0
menu=Start MINIX 3 amd64 in single-user mode:modules on; load hd0b:/boot/minix_default/mod01_ds; load hd0b:/boot/minix_default/mod02_rs; load hd0b:/boot/minix_default/mod03_pm; load hd0b:/boot/minix_default/mod04_sched; load hd0b:/boot/minix_default/mod05_vfs; load hd0b:/boot/minix_default/mod06_memory; load hd0b:/boot/minix_default/mod07_tty; load hd0b:/boot/minix_default/mod08_mib; load hd0b:/boot/minix_default/mod09_vm; load hd0b:/boot/minix_default/mod10_pfs; load hd0b:/boot/minix_default/mod11_mfs; load hd0b:/boot/minix_default/mod12_init; multiboot2 hd0b:/boot/minix_default/kernel rootdevname=c0d0p1 bootopts=-s
menu=Drop to UEFI boot prompt:prompt
clear=1
timeout=10
default=1
EOF_INSTALL_BOOTCFG
"${CROSS_TOOLS}/nbmakefs" -M "${EFIMB}m" -m "${EFIMB}m" \
	-t msdos -o F=32,c=1 "${INSTALL_EFI_IMAGE}" "${INSTALL_EFI_DIR}"
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

# The ISO9660 root is read-only after boot.  Create every static device
# needed by the CD startup path in the image itself.  rc.cd uses ram0-ram3
# to make /var, /tmp, /usr/run and /root writable, and rc.minix later frees
# the boot image through /dev/imgrd.  The /usr startup script also starts
# random, pty, LWIP, log and printer services, which require these nodes.
mkdir -p "${ROOT_DIR}/dev"
# The guided installer needs the whole target disk and its GPT partitions.
for device in console tty null zero ram mem kmem boot imgrd \
    c0d0 c0d0p0 c0d0p1 c0d0p2 c0d0p3 \
    ram0 ram1 ram2 ram3 ram4 ram5 log lp bpf bpf0 klog ptmx random urandom; do
	if [ -d "${ROOT_DIR}/dev/${device}" ]; then
		echo "${ROOT_DIR}/dev/${device} exists as a directory, expected a device node." >&2
		exit 1
	fi
	rm -f "${ROOT_DIR}/dev/${device}"
done
DEVICE_SPEC="${WORK_DIR}/uefi-devices.mtree"
cat > "${DEVICE_SPEC}" <<'EOF_DEVICES'
. type=dir uid=0 gid=0 mode=0755
./dev type=dir uid=0 gid=0 mode=0755
./dev/console type=char device=netbsd,4,0 mode=600 uid=0 gid=0
./dev/tty type=char device=netbsd,5,0 mode=666 uid=0 gid=0
./dev/null type=char device=netbsd,1,3 mode=666 uid=0 gid=0
./dev/zero type=char device=netbsd,1,5 mode=666 uid=0 gid=0
./dev/c0d0 type=block device=netbsd,3,0 mode=600 uid=0 gid=0
./dev/c0d0p0 type=block device=netbsd,3,1 mode=600 uid=0 gid=0
./dev/c0d0p1 type=block device=netbsd,3,2 mode=600 uid=0 gid=0
./dev/c0d0p2 type=block device=netbsd,3,3 mode=600 uid=0 gid=0
./dev/c0d0p3 type=block device=netbsd,3,4 mode=600 uid=0 gid=0
./dev/ram type=block device=netbsd,1,0 mode=600 uid=0 gid=2
./dev/mem type=char device=netbsd,1,1 mode=640 uid=0 gid=2
./dev/kmem type=char device=netbsd,1,2 mode=640 uid=0 gid=2
./dev/boot type=block device=netbsd,1,4 mode=600 uid=0 gid=2
./dev/imgrd type=block device=netbsd,1,6 mode=600 uid=0 gid=0
./dev/ram0 type=block device=netbsd,1,7 mode=600 uid=0 gid=0
./dev/ram1 type=block device=netbsd,1,8 mode=600 uid=0 gid=0
./dev/ram2 type=block device=netbsd,1,9 mode=600 uid=0 gid=0
./dev/ram3 type=block device=netbsd,1,10 mode=600 uid=0 gid=0
./dev/ram4 type=block device=netbsd,1,11 mode=600 uid=0 gid=0
./dev/ram5 type=block device=netbsd,1,12 mode=600 uid=0 gid=0
./dev/log type=char device=netbsd,4,15 mode=222 uid=0 gid=0
./dev/lp type=char device=netbsd,6,0 mode=200 uid=1 gid=1
./dev/bpf type=char device=netbsd,7,0 mode=600 uid=0 gid=0
./dev/bpf0 type=link link=bpf mode=0700 uid=0 gid=0
./dev/klog type=char device=netbsd,15,0 mode=600 uid=0 gid=0
./dev/ptmx type=char device=netbsd,9,0 mode=666 uid=0 gid=4
./dev/random type=char device=netbsd,16,0 mode=644 uid=0 gid=0
./dev/urandom type=char device=netbsd,16,0 mode=644 uid=0 gid=0
EOF_DEVICES

workdir_add_kernel minix_default

# efiboot automatically selects the MFS or ISO9660 partition belonging
# to the EFI boot device. Use relative paths so the CD can be enumerated as
# any hdN device, even when other disks are attached.
BOOT_MODULES="load /boot/minix_default/mod01_ds; load /boot/minix_default/mod02_rs; load /boot/minix_default/mod03_pm; load /boot/minix_default/mod04_sched; load /boot/minix_default/mod05_vfs; load /boot/minix_default/mod06_memory; load /boot/minix_default/mod07_tty; load /boot/minix_default/mod08_mib; load /boot/minix_default/mod09_vm; load /boot/minix_default/mod10_pfs; load /boot/minix_default/mod11_mfs; load /boot/minix_default/mod12_init"

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
menu=Start MINIX 3 (UEFI x86_64):modules on; ${BOOT_MODULES}; multiboot2 /boot/minix_default/kernel bootcd=1 cdproberoot=1 acpi=yes
menu=Start MINIX 3 in single-user mode:modules on; ${BOOT_MODULES}; multiboot2 /boot/minix_default/kernel bootcd=1 cdproberoot=1 bootopts=-s
menu=Start MINIX 3 on serial console:consdev com0,115200; modules on; ${BOOT_MODULES}; multiboot2 /boot/minix_default/kernel bootcd=1 cdproberoot=1 acpi=yes console=tty00
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

# Also make an ESP image for a hard-disk installation. The UEFI loader
# resolves relative boot paths against the MFS partition on the boot device.
# The installer copies this image to the target ESP.
EFI_DISK_DIR="${WORK_DIR}/efi-disk"
EFI_DISK_IMAGE="${ROOT_DIR}/efiboot-disk.img"
mkdir -p "${EFI_DISK_DIR}/EFI/BOOT"
cp "${ROOT_DIR}/usr/mdec/bootx64.efi" "${EFI_DISK_DIR}/EFI/BOOT/BOOTX64.EFI"
cat > "${EFI_DISK_DIR}/boot.cfg" <<'EOF_EFI_DISK'
banner=MINIX 3.4.0 amd64 UEFI disk boot
banner================================================
banner=
menu=Start MINIX 3 (UEFI x86_64):modules on; load /boot/minix_default/mod01_ds; load /boot/minix_default/mod02_rs; load /boot/minix_default/mod03_pm; load /boot/minix_default/mod04_sched; load /boot/minix_default/mod05_vfs; load /boot/minix_default/mod06_memory; load /boot/minix_default/mod07_tty; load /boot/minix_default/mod08_mib; load /boot/minix_default/mod09_vm; load /boot/minix_default/mod10_pfs; load /boot/minix_default/mod11_mfs; load /boot/minix_default/mod12_init; multiboot2 /boot/minix_default/kernel acpi=yes ahci=yes cttyline=0 no_smp=0 no_apic=0
menu=Start MINIX 3 in single-user mode:modules on; load /boot/minix_default/mod01_ds; load /boot/minix_default/mod02_rs; load /boot/minix_default/mod03_pm; load /boot/minix_default/mod04_sched; load /boot/minix_default/mod05_vfs; load /boot/minix_default/mod06_memory; load /boot/minix_default/mod07_tty; load /boot/minix_default/mod08_mib; load /boot/minix_default/mod09_vm; load /boot/minix_default/mod10_pfs; load /boot/minix_default/mod11_mfs; load /boot/minix_default/mod12_init; multiboot2 /boot/minix_default/kernel acpi=yes ahci=yes bootopts=-s
menu=Drop to UEFI boot prompt:prompt
clear=1
timeout=5
default=1
EOF_EFI_DISK
"${CROSS_TOOLS}/nbmakefs" -M "${EFIMB}m" -m "${EFIMB}m" \
	-t msdos -o F=32,c=1 "${EFI_DISK_IMAGE}" "${EFI_DISK_DIR}"

mkdir -p "$(dirname "${IMG}")"
IMG_DIR=$(cd "$(dirname "${IMG}")" && pwd)
IMG_PATH="${IMG_DIR}/$(basename "${IMG}")"
NBMAKEFS="$(cd "${CROSS_TOOLS}" && pwd)/nbmakefs"
rm -f "${IMG_PATH}"
echo "Writing UEFI El Torito ISO with nbmakefs: ${IMG_PATH}"
(
	cd "${ROOT_DIR}"
	"${NBMAKEFS}" -N "${ROOT_DIR}/etc" -t cd9660 \
		-F "${DEVICE_SPEC}" \
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
