#!/bin/sh
# MINIX 3.4.0 amd64 UEFI/GPT installer.
# This script intentionally requires an explicit confirmation before it
# repartitions a disk: every existing partition on the selected disk is lost.

set -eu

PATH=/sbin:/bin:/usr/sbin:/usr/bin
export PATH

SETS_DIR=${SETS_DIR:-/usr/amd64/binary/sets}
EFI_SIZE=64m
EFI_IMAGE=${EFI_IMAGE:-/install-efiboot.img}
TARGET_DISK=

usage()
{
	cat <<'USAGE'
Usage: minix-install [--disk c0d0] [--help]

Install MINIX 3.4.0 amd64 on a UEFI x86-64 computer.
The installer creates a GPT, a 64 MiB EFI System Partition, and one
MINIX MFS root partition. ALL DATA AND PARTITIONS on the selected disk
will be erased. The disk must not be mounted.

The release sets are read from /usr/amd64/binary/sets by default.
Override this directory with SETS_DIR=/path/to/sets.
USAGE
}

fail()
{
	echo "minix-install: $*" >&2
	exit 1
}

need_command()
{
	command -v "$1" >/dev/null 2>&1 || fail "Required command not found: $1"
}

while [ "$#" -gt 0 ]; do
	case "$1" in
	--disk)
		[ "$#" -ge 2 ] || fail "--disk requires a device name"
		TARGET_DISK=$2
		shift 2
		;;
	--help|-h)
		usage
		exit 0
		;;
	*)
		usage >&2
		exit 2
		;;
	esac
done

[ "$(id -u)" = 0 ] || fail "Run this installer as root."

for command in dd gpt mkfs.mfs mount umount pax cp mkdir mv rm rmdir chmod sync grep id sleep wc; do
	need_command "$command"
done

[ -d "$SETS_DIR" ] || fail "Release sets directory not found: $SETS_DIR"
[ -s "$SETS_DIR/minix-base.tgz" ] || fail "Missing release set: $SETS_DIR/minix-base.tgz"
[ -s "$SETS_DIR/minix-kernel.tgz" ] || fail "Missing release set: $SETS_DIR/minix-kernel.tgz"
[ -s "$EFI_IMAGE" ] || fail "Prebuilt UEFI system partition image not found: $EFI_IMAGE"
[ "$(wc -c < "$EFI_IMAGE")" -eq 67108864 ] || \
	fail "UEFI system partition image must be exactly 64 MiB: $EFI_IMAGE"

if [ -z "$TARGET_DISK" ]; then
	cat <<'PROMPT'
Enter the whole-disk MINIX device (for example c0d0, not a partition).
You can inspect available devices with: ls /dev/c*d*
PROMPT
	printf 'Target disk: '
	IFS= read -r TARGET_DISK || fail "No disk was selected."
fi

case "$TARGET_DISK" in
/dev/*) TARGET_DISK=${TARGET_DISK#/dev/} ;;
esac
case "$TARGET_DISK" in
''|*[!A-Za-z0-9]*) fail "Invalid disk name: $TARGET_DISK" ;;
c0d0) ;;
*) fail "The prebuilt UEFI menu targets c0d0p1; connect the install disk as c0d0." ;;
esac

DISK_DEVICE=/dev/$TARGET_DISK
[ -b "$DISK_DEVICE" ] || [ -c "$DISK_DEVICE" ] || \
	fail "Disk device does not exist: $DISK_DEVICE"

if mount | grep -F "$TARGET_DISK" >/dev/null 2>&1; then
	fail "A filesystem on $DISK_DEVICE is mounted. Unmount it before installing."
fi

cat <<CONFIRM

WARNING: ALL DATA ON $DISK_DEVICE WILL BE DESTROYED.
The installer will replace its partition table with:
  1. 64 MiB EFI System Partition (FAT32)
  2. MINIX MFS root partition (remaining space)

CONFIRM
printf 'To continue, type ERASE %s: ' "$TARGET_DISK"
IFS= read -r ANSWER || fail "Installation cancelled."
[ "$ANSWER" = "ERASE $TARGET_DISK" ] || fail "Confirmation did not match; disk was not changed."

EFI_PART=/dev/${TARGET_DISK}p0
ROOT_PART=/dev/${TARGET_DISK}p1

# gpt create -f removes any existing MBR/GPT. Do not run this before the
# explicit confirmation above.
echo "Creating GPT on $DISK_DEVICE..."
gpt create -f "$DISK_DEVICE"
gpt add -a 1m -s "$EFI_SIZE" -t efi -l "EFI system" "$DISK_DEVICE"
gpt add -a 1m -t mfs -l MINIX "$DISK_DEVICE"
# Match the amd64 live-image layout: GPT entry 1 is the ESP and entry 2
# is the root MFS partition selected by the MINIX UEFI boot monitor.
gpt set -a bootme -i 2 "$DISK_DEVICE"

wait_for_device()
{
	device=$1
	attempt=0
	while [ "$attempt" -lt 10 ]; do
		if [ -b "$device" ] || [ -c "$device" ]; then
			return 0
		fi
		attempt=$((attempt + 1))
		sleep 1
	done
	fail "Partition device was not created: $device"
}

wait_for_device "$EFI_PART"
wait_for_device "$ROOT_PART"

echo "Writing the FAT32 UEFI System Partition image..."
dd if="$EFI_IMAGE" of="$EFI_PART"
sync
echo "Creating MINIX MFS root filesystem..."
mkfs.mfs "$ROOT_PART"

WORK_DIR=/tmp/minix-install.$$
ROOT_MNT=$WORK_DIR/root
ROOT_MOUNTED=no

cleanup()
{
	if [ "$ROOT_MOUNTED" = yes ]; then
		umount "$ROOT_MNT" || echo "minix-install: could not unmount $ROOT_MNT" >&2
	fi
	rmdir "$ROOT_MNT" "$WORK_DIR" 2>/dev/null || true
}
trap cleanup 0
trap 'exit 1' HUP INT TERM

mkdir -p "$ROOT_MNT"
mount -t mfs "$ROOT_PART" "$ROOT_MNT"
ROOT_MOUNTED=yes
chmod 0755 "$ROOT_MNT"

install_set()
{
	set_name=$1
	echo "Installing $set_name..."
	(cd "$ROOT_MNT" && pax -rz -pe -f "$SETS_DIR/$set_name.tgz")
}

install_set minix-base
if [ -s "$SETS_DIR/minix-comp.tgz" ]; then
	install_set minix-comp
elif [ -s "$SETS_DIR/minix-comp-gcc.tgz" ]; then
	install_set minix-comp-gcc
fi
for set in minix-games minix-man minix-tests tests; do
	if [ -s "$SETS_DIR/$set.tgz" ]; then
		install_set "$set"
	else
		echo "Skipping optional release set $set (not present)."
	fi
done

# The kernel set stores its files under boot/minix/.temp. Match the layout
# used by the amd64 live-image build so the EFI boot monitor can load them.
echo "Installing amd64 kernel and boot modules..."
(cd "$ROOT_MNT" && pax -rz -pe -f "$SETS_DIR/minix-kernel.tgz")
[ -d "$ROOT_MNT/boot/minix/.temp" ] || \
	fail "The kernel set did not contain boot/minix/.temp."
mkdir -p "$ROOT_MNT/boot/minix_default"
for file in "$ROOT_MNT"/boot/minix/.temp/*; do
	[ -f "$file" ] || continue
	mv "$file" "$ROOT_MNT/boot/minix_default/"
done
rm -rf "$ROOT_MNT/boot/minix/.temp"

for file in kernel mod01_ds mod02_rs mod03_pm mod04_sched mod05_vfs \
    mod06_memory mod07_tty mod08_mib mod09_vm mod10_pfs mod11_mfs mod12_init; do
	[ -s "$ROOT_MNT/boot/minix_default/$file" ] || \
		fail "Missing installed boot file: /boot/minix_default/$file"
done

cat > "$ROOT_MNT/etc/fstab" <<FSTAB
/dev/${TARGET_DISK}p1	/		mfs	rw	1 1
none		/sys		devman	rw,rslabel=devman	0 0
none		/dev/pts	ptyfs	rw,rslabel=ptyfs	0 0
FSTAB
[ -f "$ROOT_MNT/etc/rc.conf" ] || fail "The minix-base set did not install /etc/rc.conf."
cat >> "$ROOT_MNT/etc/rc.conf" <<'RCCONF'

# The installer creates a single MFS root partition and no swap partition.
rc_configured=YES
no_swap=YES
savecore=NO
RCCONF

# The EFI image already contains the boot loader and boot.cfg.

sync
echo
echo "MINIX 3.4.0 amd64 has been installed on $DISK_DEVICE."
echo "The disk uses UEFI/GPT, a FAT32 EFI System Partition, and an MFS root."
echo "Remove the installation media and reboot in UEFI mode."
