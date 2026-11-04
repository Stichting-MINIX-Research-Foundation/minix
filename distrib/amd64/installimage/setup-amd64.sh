#!/bin/sh
# Guided UEFI/GPT installer for MINIX 3.4.0 amd64.
# The selected disk is repartitioned and all data on it is destroyed.

PATH=/bin:/sbin:/usr/bin:/usr/sbin
export PATH

# The CD ISO stages release archives below /usr/amd64; the generic
# install-image target copies the machine release tree to /amd64.
SETS_DIR=/usr/amd64/binary/sets
[ -d "$SETS_DIR" ] || SETS_DIR=/amd64/binary/sets
EFI_IMAGE=/efiboot-disk.img
EFI_SIZE_MB=64
MIN_DISK_MB=2048
MOUNT_POINT=/mnt

fail()
{
	echo "\nERROR: $*" >&2
	echo "No further installation steps will be performed." >&2
	exit 1
}

ask_yes_no()
{
	question=$1
	default=$2
	if [ "$default" = yes ]; then
		printf "%s [Y/n] " "$question"
	else
		printf "%s [y/N] " "$question"
	fi
	read answer || return 1
	case "$answer" in
		[Yy]|[Yy][Ee][Ss]) return 0 ;;
		[Nn]|[Nn][Oo]) return 1 ;;
		"") [ "$default" = yes ] && return 0 || return 1 ;;
		*) return 1 ;;
	esac
}

[ "`id -u 2>/dev/null`" = 0 ] || fail "Run the installer as root."
command -v devsize >/dev/null 2>&1 || fail "The devsize utility is not available."
command -v printroot >/dev/null 2>&1 || fail "The printroot utility is not available."
[ -d "$SETS_DIR" ] || fail "Release sets not found at $SETS_DIR. Boot the amd64 installation media."
[ -s "$SETS_DIR/minix-base.tgz" ] || fail "Required set minix-base.tgz is missing."
[ -s "$SETS_DIR/minix-kernel.tgz" ] || fail "Required set minix-kernel.tgz is missing."

cat <<'BANNER'

MINIX 3.4.0 — amd64 UEFI installer
==================================

This installer creates a GPT disk with a 64 MiB EFI System Partition and
one MINIX MFS root partition. It installs the amd64 release sets and the
UEFI boot files.

WARNING: the selected disk will be repartitioned. ALL DATA on that disk,
including every existing operating system and partition, will be erased.

BANNER

if ! ask_yes_no "Start the installation?" no; then
	echo "Installation cancelled. The running system was not changed."
	exit 0
fi

printf "\nAvailable disk devices:\n"
found_disk=no
for candidate in /dev/c[0-9]d[0-9]; do
	if [ -b "$candidate" ]; then
		size=`devsize "$candidate" 2>/dev/null` || size=unknown
		if [ "$size" != unknown ] && [ -n "$size" ]; then
			case "$size" in
				*[!0-9]*) size=unknown ;;
				*) size="$((size / 2048)) MiB" ;;
			esac
		fi
		printf "  %-14s %s\n" "$candidate" "$size"
		found_disk=yes
	fi
done
[ "$found_disk" = yes ] || fail "No disk devices such as /dev/c0d0 were found."

printf "\nEnter the whole target disk (for example c0d0): "
read disk || fail "Could not read the target disk."
case "$disk" in
	/dev/*) disk=${disk#/dev/} ;;
esac
case "$disk" in
	c[0-9]d[0-9]) ;;
	*) fail "Invalid disk name '$disk'. Use a whole disk such as c0d0 (not a partition)." ;;
esac

diskdev=/dev/$disk
espdev=/dev/${disk}p0
rootdev=/dev/${disk}p1
[ -b "$diskdev" ] || fail "$diskdev is not a block device."

size=`devsize "$diskdev" 2>/dev/null` || fail "Cannot determine the size of $diskdev."
case "$size" in
	''|*[!0-9]*) fail "Unexpected disk size reported for $diskdev: $size" ;;
esac
diskmb=$((size / 2048))
[ "$diskmb" -ge "$MIN_DISK_MB" ] || fail "$diskdev has ${diskmb} MiB; at least ${MIN_DISK_MB} MiB is required."

# Never destroy the disk from which the installer is currently running.
running_root=`printroot -r 2>/dev/null || true`
running_disk=`echo "$running_root" | sed 's/p[0-9][0-9]*$//'`
if [ -n "$running_disk" ] && [ "$running_disk" = "$diskdev" ]; then
	fail "$diskdev contains the running installer. Boot from a separate USB/CD device and choose another disk."
fi

if mount 2>/dev/null | grep -F "$diskdev" >/dev/null 2>&1; then
	fail "A filesystem on $diskdev is mounted. Unmount it before installing."
fi

cat <<END_LAYOUT

Target disk: $diskdev (${diskmb} MiB)
Partition layout:
  $espdev  ${EFI_SIZE_MB} MiB  EFI System Partition (FAT32)
  $rootdev  remaining space  MINIX MFS root

END_LAYOUT
printf "To confirm data destruction, type the disk name exactly (%s): " "$disk"
read confirmation || fail "Could not read confirmation."
[ "$confirmation" = "$disk" ] || fail "Confirmation did not match. Nothing was written to the target disk."

# Validate the selected release archives before changing the target disk.
# Either LLVM or GCC builds may provide the compiler/toolchain set name.
set_list="minix-base minix-comp minix-comp-gcc minix-games minix-man minix-tests tests minix-kernel"
for set_name in $set_list; do
	case "$set_name" in
		minix-base|minix-kernel)
			[ -s "$SETS_DIR/$set_name.tgz" ] || fail "Required release set $set_name.tgz is missing."
			;;
	esac
done

command -v gpt >/dev/null 2>&1 || fail "The gpt utility is not available."
command -v mkfs.mfs >/dev/null 2>&1 || fail "The mkfs.mfs utility is not available."
command -v pax >/dev/null 2>&1 || fail "The pax utility is not available."
command -v dd >/dev/null 2>&1 || fail "The dd utility is not available."
# Resolve the UEFI boot image before any destructive operation.
# CD media carries a disk-specific ESP image; the install-image USB can reuse
# its own EFI partition, provided the target is a different disk.
if [ -s "$EFI_IMAGE" ]; then
	efi_source=$EFI_IMAGE
else
	if [ -z "$running_disk" ] || [ ! -b "${running_disk}p0" ]; then
		fail "Cannot locate the running install media's EFI partition, and $EFI_IMAGE is not present."
	fi
	efi_source=${running_disk}p0
fi
[ -s "$efi_source" ] || [ -b "$efi_source" ] || fail "UEFI boot source $efi_source is not readable."

printf "\nCreating GPT on %s...\n" "$diskdev"
gpt "$diskdev" create -f || fail "Could not create the GPT on $diskdev."
gpt "$diskdev" add -a 1m -s ${EFI_SIZE_MB}m -t efi -l "EFI System" || fail "Could not create the EFI System Partition."
gpt "$diskdev" add -a 1m -t mfs -l "MINIX root" || fail "Could not create the MINIX MFS partition."

# Wait for MINIX's disk driver/devman to publish the new GPT wedges.
tries=0
while [ ! -b "$espdev" ] || [ ! -b "$rootdev" ]; do
	tries=$((tries + 1))
	[ "$tries" -lt 15 ] || fail "GPT was written, but the partition devices $espdev and $rootdev did not appear."
	sleep 1
done

printf "\nCreating the MINIX root filesystem...\n"
mkfs.mfs "$rootdev" || fail "Could not create the MFS filesystem on $rootdev."

printf "Installing UEFI boot files from %s...\n" "$efi_source"
dd if="$efi_source" of="$espdev" || fail "Could not copy the UEFI boot files to $espdev."
sync

mkdir -p "$MOUNT_POINT" || fail "Could not create $MOUNT_POINT."
mount "$rootdev" "$MOUNT_POINT" || fail "Could not mount the new root filesystem."

printf "\nExtracting amd64 release sets (this may take several minutes)...\n"
for set_name in $set_list; do
	set_file=$SETS_DIR/$set_name.tgz
	[ -s "$set_file" ] || continue
	printf "  Installing %s...\n" "$set_name"
	(cd "$MOUNT_POINT" && pax -rz -pe -f "$set_file") || {
		umount "$MOUNT_POINT" >/dev/null 2>&1 || true
		fail "Failed to extract $set_name.tgz."
	}
done

# The kernel release archive is staged under boot/minix/.temp by the
# release tooling. Move it to the directory referenced by the UEFI boot.cfg.
if [ ! -s "$MOUNT_POINT/boot/minix_default/kernel" ]; then
	[ -d "$MOUNT_POINT/boot/minix/.temp" ] || {
		umount "$MOUNT_POINT" >/dev/null 2>&1 || true
		fail "The kernel set did not install boot/minix/.temp."
	}
	rm -rf "$MOUNT_POINT/boot/minix_default"
	cp -r "$MOUNT_POINT/boot/minix/.temp" "$MOUNT_POINT/boot/minix_default" || {
		umount "$MOUNT_POINT" >/dev/null 2>&1 || true
		fail "Could not stage the kernel and boot modules under /boot/minix_default."
	}
	rm -rf "$MOUNT_POINT/boot/minix/.temp"
fi
[ -s "$MOUNT_POINT/boot/minix_default/kernel" ] || {
	umount "$MOUNT_POINT" >/dev/null 2>&1 || true
	fail "The installed kernel is missing from /boot/minix_default."
}

cat > "$MOUNT_POINT/etc/fstab" <<END_FSTAB
$rootdev	/		mfs	rw		1 1
none		/sys		devman	rw,rslabel=devman	0 0
none		/dev/pts	ptyfs	rw,rslabel=ptyfs	0 0
END_FSTAB

# Enable multiuser startup for the freshly installed root filesystem.
[ -f "$MOUNT_POINT/etc/rc.conf" ] || {
	umount "$MOUNT_POINT" >/dev/null 2>&1 || true
	fail "The base set did not install /etc/rc.conf."
}
cat >> "$MOUNT_POINT/etc/rc.conf" <<END_RCCONF

# Settings for the amd64 single-root UEFI installation.
rc_configured=YES
no_swap=YES
savecore=NO
END_RCCONF

# The image builder uses the same MAKEDEV script to populate /dev.
[ -x "$MOUNT_POINT/dev/MAKEDEV" ] || {
	umount "$MOUNT_POINT" >/dev/null 2>&1 || true
	fail "The base set did not install /dev/MAKEDEV."
}
(cd "$MOUNT_POINT/dev" && ./MAKEDEV -s all ipty imgrd) || {
	umount "$MOUNT_POINT" >/dev/null 2>&1 || true
	fail "MAKEDEV could not create the target device nodes."
}

sync
umount "$MOUNT_POINT" || fail "Could not unmount $MOUNT_POINT cleanly."
sync

cat <<END_SUCCESS

MINIX 3.4.0 amd64 has been installed successfully on $diskdev.

The disk now contains:
  EFI System Partition: $espdev
  MINIX root filesystem: $rootdev

Remove the installation media, then reboot and select the disk's UEFI
boot entry. The installer does not change the firmware boot order.

END_SUCCESS
exit 0
