#!/bin/sh
# Guided amd64 equivalent of minix/commands/setup/setup.sh.
# UEFI requires the boot root to remain GPT partition 2 (/dev/c0d0p1).
set -eu

PATH=/sbin:/bin:/usr/sbin:/usr/bin
export PATH
SETS_DIR=$(env | sed -n 's/^SETS_DIR=//p')
[ -n "$SETS_DIR" ] || SETS_DIR=/usr/amd64/binary/sets
EFI_IMAGE=$(env | sed -n 's/^EFI_IMAGE=//p')
[ -n "$EFI_IMAGE" ] || EFI_IMAGE=/install-efiboot.img
# This copy is staged by x86_64_uefi_cdimage.sh and is newer than the
# MAKEDEV script shipped in the minix-base release set.
MAKEDEV_SOURCE=/minix-MAKEDEV
DISK=c0d0
ESP=/dev/$DISK"p0"
ROOT=/dev/$DISK"p1"
PROBE=/tmp/minix-install-probe.$$
MNT=/mnt
EFI_MB=64
ROOT_MB=128
BLOCK_BYTES=4096
HOME_ENABLED=no
PRESERVE_HOME=no
HOME_PART=
USR_PART=
MOUNTED_ROOT=no
MOUNTED_USR=no
MOUNTED_HOME=no

fail() { echo "minix-install: $*" >&2; exit 1; }
need() { command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"; }

usage() {
	cat <<'EOF'
Usage: minix-install [--disk c0d0] [--help]
Install MINIX 3.4.0 amd64. The current UEFI boot menu requires the target
disk to be c0d0 and the installed root to be c0d0p1. Automatic mode replaces
the whole disk. Expert mode reuses an existing GPT with ESP p0 and root p1.
Release sets: /usr/amd64/binary/sets (override with SETS_DIR).
EOF
}

ask_yes_no() {
	while :; do
		printf '%s [y/N] ' "$1"
		read answer || fail "input ended"
		case "$answer" in
			[Yy]|[Yy][Ee][Ss]) return 0 ;;
			[Nn]|[Nn][Oo]|'') return 1 ;;
			*) echo "Please answer yes or no." ;;
		esac
	done
}

ask_number() {
	prompt=$1 min=$2 max=$3 default=$4
	while :; do
		printf '%s (%s-%s) [%s] ' "$prompt" "$min" "$max" "$default"
		read value || fail "input ended"
		[ -n "$value" ] || value=$default
		case "$value" in *[!0-9]*|'') echo "Enter a whole number." ;; *)
			if [ "$value" -ge "$min" ] && [ "$value" -le "$max" ]; then
				NUMBER=$value
				return
			fi
			echo "Enter a value between $min and $max."
			;;
		esac
	done
}

wait_for_device() {
	device=$1
	attempt=0
	while [ "$attempt" -lt 15 ]; do
		if [ -b "$device" ] || [ -c "$device" ]; then return 0; fi
		attempt=$((attempt + 1))
		sleep 1
	done
	fail "partition device was not created: $device"
}

cleanup() {
	[ "$MOUNTED_HOME" = yes ] && umount "$MNT/home" >/dev/null 2>&1 || :
	[ "$MOUNTED_USR" = yes ] && umount "$MNT/usr" >/dev/null 2>&1 || :
	[ "$MOUNTED_ROOT" = yes ] && umount "$MNT" >/dev/null 2>&1 || :
	rmdir "$PROBE" >/dev/null 2>&1 || :
}
trap cleanup 0
trap 'exit 1' HUP INT TERM

while [ "$#" -gt 0 ]; do
	case "$1" in
		--disk) [ "$#" -ge 2 ] || fail "--disk requires a disk name"; DISK=$2; shift 2 ;;
		--help|-h) usage; exit 0 ;;
		*) usage >&2; exit 2 ;;
	esac
done

[ "$(id -u)" = 0 ] || fail "run this installer as root"
[ "$DISK" = c0d0 ] || fail "the current UEFI boot menu requires the target disk to be c0d0"
for cmd in id gpt devsize mkfs.mfs mount umount pax dd sync wc grep sleep mkdir rmdir chmod chroot cp mv rm mknod awk cut sed uname basename env; do need "$cmd"; done
[ -x "$MAKEDEV_SOURCE" ] || fail "updated MAKEDEV script is missing from installation media: $MAKEDEV_SOURCE"
[ -d "$SETS_DIR" ] || fail "release set directory not found: $SETS_DIR"
[ -s "$SETS_DIR/minix-base.tgz" ] || fail "minix-base.tgz is missing"
[ -s "$SETS_DIR/minix-kernel.tgz" ] || fail "minix-kernel.tgz is missing"
[ -s "$EFI_IMAGE" ] || fail "EFI system partition image not found: $EFI_IMAGE"
[ "$(wc -c < "$EFI_IMAGE")" -eq 67108864 ] || fail "EFI image must be exactly 64 MiB"
DISKDEV=/dev/$DISK
[ -b "$DISKDEV" ] || [ -c "$DISKDEV" ] || fail "$DISKDEV is not a disk device"
if mount | grep -F "$DISKDEV" >/dev/null 2>&1; then fail "$DISKDEV has a mounted filesystem"; fi
SECTORS=$(devsize "$DISKDEV") || fail "cannot determine disk size"
DISK_MB=$((SECTORS / 2048))
# The .size manifests contain byte counts. Estimate the unpacked /usr
# requirement and leave 128 MiB for files created during installation and
# later updates, following the 32-bit setup script's size calculation.
USR_BYTES=0
for size_file in "$SETS_DIR"/*.size; do
	[ -f "$size_file" ] || continue
	size_value=$(awk '{ sum += $1 } END { printf "%.0f", sum }' "$size_file")
	case "$size_value" in ''|*[!0-9]*) ;; *) USR_BYTES=$((USR_BYTES + size_value)) ;; esac
done
if [ "$USR_BYTES" -gt 0 ]; then
	USR_MB=$(((USR_BYTES + 1048575) / 1048576 + 128))
else
	USR_MB=1024
fi
MIN_DISK_MB=$((EFI_MB + ROOT_MB + USR_MB + 3))
[ "$DISK_MB" -ge "$MIN_DISK_MB" ] || fail "at least $MIN_DISK_MB MiB is required for the release sets"

echo
echo "MINIX 3.4.0 amd64 — guided UEFI installation"
echo "Step 1: Select keyboard type"
KEYMAP=
if [ -d /usr/lib/keymaps ] && command -v loadkeys >/dev/null 2>&1; then
	for map in /usr/lib/keymaps/*.map; do [ -f "$map" ] && echo "  $(basename "$map")" | sed 's/\.map$//'; done
	while :; do
		printf 'Keyboard type? [us-std] '
		read KEYMAP || fail "input ended"
		[ -n "$KEYMAP" ] || KEYMAP=us-std
		case "$KEYMAP" in */*|*'..'*) echo "Enter a map name, not a path."; KEYMAP=; continue ;; esac
		if loadkeys "/usr/lib/keymaps/$KEYMAP.map" 2>/dev/null; then break; fi
		echo "Invalid keyboard map: $KEYMAP"
		KEYMAP=
	done
else
	echo "Keyboard-map selection is unavailable; keeping the current map."
fi

echo
echo "Step 2: Select full distribution"
echo "All available amd64 release sets will be installed."

echo
echo "Step 3: Create or select a MINIX partition"
echo "Automatic mode replaces the complete disk with a new GPT."
echo "Expert mode reuses an existing GPT with these device names:"
echo "  c0d0p0  EFI System Partition (at least 64 MiB)"
echo "  c0d0p1  MINIX MFS root (must remain p1 for UEFI boot)"
printf "Press ENTER for automatic mode, or type 'expert': "
read MODE || fail "input ended"
case "$MODE" in
	'') MODE=automatic ;;
	expert|EXPERT) MODE=expert ;;
	*) fail "choose automatic mode or type expert" ;;
esac

if [ "$MODE" = automatic ]; then
	MAX_HOME=$((DISK_MB - EFI_MB - ROOT_MB - USR_MB - 3))
	[ "$MAX_HOME" -ge 0 ] || fail "disk is too small for EFI, root and /usr"
	echo
	echo "Step 4: Select the size of /home"
	echo "MINIX will use 64 MiB for EFI, 128 MiB for root and approximately $USR_MB MiB for /usr."
	DEFAULT_HOME=$((MAX_HOME / 5))
	ask_number "How big do you want /home in MiB (0 disables /home)?" 0 "$MAX_HOME" "$DEFAULT_HOME"
	HOME_MB=$NUMBER
	if [ "$HOME_MB" -gt 0 ]; then
		HOME_ENABLED=yes
		HOME_PART=/dev/$DISK"p2"
		USR_PART=/dev/$DISK"p3"
	else
		USR_PART=/dev/$DISK"p2"
	fi
	printf '\nTarget: %s (%s MiB)\nESP: %s (%s MiB)\nRoot: %s (%s MiB)\n' "$DISKDEV" "$DISK_MB" "$ESP" "$EFI_MB" "$ROOT" "$ROOT_MB"
	[ "$HOME_ENABLED" = yes ] && echo "Home: $HOME_PART ($HOME_MB MiB)"
	echo "Usr: $USR_PART (remaining space)"
	echo "All existing data on $DISKDEV will be erased."
	printf "To continue, type ERASE %s: " "$DISK"
	read CONFIRM || fail "installation cancelled"
	[ "$CONFIRM" = "ERASE $DISK" ] || fail "confirmation did not match; disk unchanged"
	gpt create -f "$DISKDEV" || fail "could not create GPT"
	gpt add -a 1m -i 1 -s "$EFI_MB"m -t efi -l "EFI system" "$DISKDEV" || fail "could not create ESP"
	gpt add -a 1m -i 2 -s "$ROOT_MB"m -t mfs -l "MINIX root" "$DISKDEV" || fail "could not create root"
	if [ "$HOME_ENABLED" = yes ]; then
		gpt add -a 1m -i 3 -s "$HOME_MB"m -t mfs -l "MINIX home" "$DISKDEV" || fail "could not create /home"
		gpt add -a 1m -i 4 -t mfs -l "MINIX usr" "$DISKDEV" || fail "could not create /usr"
	else
		gpt add -a 1m -i 3 -t mfs -l "MINIX usr" "$DISKDEV" || fail "could not create /usr"
	fi
	gpt set -a bootme -i 2 "$DISKDEV" || fail "could not mark root bootable"
else
	echo
	echo "Existing GPT partition table:"
	gpt show -l "$DISKDEV" || fail "could not read GPT; prepare a GPT and retry"
	[ -b "$ESP" ] || [ -c "$ESP" ] || fail "expert mode requires $ESP"
	[ -b "$ROOT" ] || [ -c "$ROOT" ] || fail "expert mode requires $ROOT"
	if ask_yes_no "Is c0d0p2 reserved for a separate /home partition?" no; then
		HOME_ENABLED=yes
		HOME_PART=/dev/$DISK"p2"
		USR_PART=/dev/$DISK"p3"
	else
		USR_PART=/dev/$DISK"p2"
	fi
	[ -b "$USR_PART" ] || [ -c "$USR_PART" ] || fail "$USR_PART does not exist"
	HOME_MB=0
	if [ "$HOME_ENABLED" = yes ]; then
		[ -b "$HOME_PART" ] || [ -c "$HOME_PART" ] || fail "$HOME_PART does not exist"
		mkdir -p "$PROBE"
		if mount -r "$HOME_PART" "$PROBE" >/dev/null 2>&1; then
			umount "$PROBE" || fail "could not unmount /home probe"
			echo "An existing MFS /home partition was found."
			echo "Type F for a full install or R to reinstall while preserving /home."
			while :; do
				printf "(F)ull or (R)einstall? [R] "
				read CHOICE || fail "input ended"
				case "$CHOICE" in
					''|[Rr]*) PRESERVE_HOME=yes; break ;;
					[Ff]*) PRESERVE_HOME=no; break ;;
					*) echo "Please enter F or R." ;;
				esac
			done
		else
			echo "No mountable MFS /home found; it will be formatted."
		fi
		rmdir "$PROBE"
	fi
	ESP_SECTORS=$(devsize "$ESP") || fail "cannot determine ESP size"
	ROOT_SECTORS=$(devsize "$ROOT") || fail "cannot determine root size"
	USR_SECTORS=$(devsize "$USR_PART") || fail "cannot determine /usr size"
	[ "$ESP_SECTORS" -ge 131072 ] || fail "ESP must be at least 64 MiB"
	[ "$ROOT_SECTORS" -ge $((ROOT_MB * 2048)) ] || fail "root partition must be at least 128 MiB"
	[ "$USR_SECTORS" -ge $((USR_MB * 2048)) ] || fail "/usr partition must be at least $USR_MB MiB"
	echo
	echo "Step 4: Reinstall choice"
	echo "The existing GPT will be kept. Only the ESP, root and /usr will be overwritten."
	[ "$PRESERVE_HOME" = yes ] && echo "Existing /home data will be preserved."
	printf "To continue, type INSTALL %s: " "$ROOT"
	read CONFIRM || fail "installation cancelled"
	[ "$CONFIRM" = "INSTALL $ROOT" ] || fail "confirmation did not match; no partition changed"
fi

if [ "$MODE" = automatic ]; then
	wait_for_device "$ESP"
	wait_for_device "$ROOT"
	[ "$HOME_ENABLED" != yes ] || wait_for_device "$HOME_PART"
	wait_for_device "$USR_PART"
fi

echo
if [ "$PRESERVE_HOME" = yes ]; then
	echo "Step 5: Reinstall uses the default 4 KiB filesystem block size."
	BLOCK_BYTES=4096
else
	echo "Step 5: Select a filesystem block size"
	echo "The default filesystem block size is 4 KiB."
	ask_number "Block size in KiB" 1 64 4
	BLOCK_BYTES=$((NUMBER * 1024))
fi

# Validate every command needed for installation before formatting anything.
need mkfs.mfs
need dd
need pax
echo
echo "Creating filesystems..."
mkfs.mfs "$ROOT" || fail "could not create root MFS filesystem"
if [ "$HOME_ENABLED" = yes ] && [ "$PRESERVE_HOME" != yes ]; then
	mkfs.mfs -B "$BLOCK_BYTES" "$HOME_PART" || fail "could not create /home MFS filesystem"
fi
mkfs.mfs -B "$BLOCK_BYTES" "$USR_PART" || fail "could not create /usr MFS filesystem"
dd if="$EFI_IMAGE" of="$ESP" bs=64k || fail "could not write UEFI system partition image"
sync

echo
echo "Step 6: Installing release sets"
mkdir -p "$MNT"
mount "$ROOT" "$MNT" || fail "could not mount root filesystem"
MOUNTED_ROOT=yes
chmod 0755 "$MNT"
mkdir -p "$MNT/usr" "$MNT/home"
mount "$USR_PART" "$MNT/usr" || fail "could not mount /usr"
MOUNTED_USR=yes
if [ "$HOME_ENABLED" = yes ]; then
	mount "$HOME_PART" "$MNT/home" || fail "could not mount /home"
	MOUNTED_HOME=yes
fi

install_set() {
	set_name=$1
	set_file=$SETS_DIR/$set_name.tgz
	[ -s "$set_file" ] || return 0
	echo "Extracting $set_name..."
	# The destination was just formatted, so do not stage files under
	# temporary names and rename them into place during extraction.
	(cd "$MNT" && PAX_DIRECT_EXTRACT=1 pax -rzv -pe -f "$set_file") || fail "failed to extract $set_name"
}
# Install every release set on the media, as the 32-bit setup script does.
# The kernel set is extracted last so its temporary boot files can be moved
# into the path expected by the prebuilt EFI boot menu.
for set_file in "$SETS_DIR"/*.tgz; do
	[ -s "$set_file" ] || continue
	set_name=$(basename "$set_file" .tgz)
	[ "$set_name" = minix-kernel ] && continue
	install_set "$set_name"
done
echo "Installing amd64 kernel and boot modules..."
install_set minix-kernel
[ -d "$MNT/boot/minix/.temp" ] || fail "kernel set did not contain boot/minix/.temp"
mkdir -p "$MNT/boot/minix_default"
for file in "$MNT"/boot/minix/.temp/*; do
	[ -f "$file" ] || continue
	mv "$file" "$MNT/boot/minix_default/"
done
rm -rf "$MNT/boot/minix/.temp"
for file in kernel mod01_ds mod02_rs mod03_pm mod04_sched mod05_vfs mod06_memory mod07_tty mod08_mib mod09_vm mod10_pfs mod11_mfs mod12_init; do
	[ -s "$MNT/boot/minix_default/$file" ] || fail "missing boot file: $file"
done
[ -d "$MNT/etc" ] || fail "base set did not install /etc"
cat > "$MNT/etc/fstab" <<EOF
$ROOT	/	mfs	rw	0	1
$USR_PART	/usr	mfs	rw	0	2
EOF
if [ "$HOME_ENABLED" = yes ]; then echo "$HOME_PART	/home	mfs	rw	0	2" >> "$MNT/etc/fstab"; fi
cat >> "$MNT/etc/fstab" <<'EOF'
none	/sys	devman	rw,rslabel=devman	0	0
none	/dev/pts	ptyfs	rw,rslabel=ptyfs	0	0
EOF
[ -f "$MNT/etc/rc.conf" ] || fail "base set did not install /etc/rc.conf"
cat >> "$MNT/etc/rc.conf" <<'EOF'

# Settings for the amd64 UEFI installation.
rc_configured=YES
no_swap=YES
savecore=NO
EOF
[ -x "$MNT/dev/MAKEDEV" ] || fail "base set did not install /dev/MAKEDEV"
# The extracted minix-base set may contain an older MAKEDEV. Replace it with
# the version carried separately on the installation ISO before creating
# target device nodes.
MAKEDEV_TMP="$MNT/dev/.MAKEDEV.tmp"
cp "$MAKEDEV_SOURCE" "$MAKEDEV_TMP" || fail "could not copy updated MAKEDEV"
chmod 0755 "$MAKEDEV_TMP" || fail "could not make updated MAKEDEV executable"
mv -f "$MAKEDEV_TMP" "$MNT/dev/MAKEDEV" || fail "could not install updated MAKEDEV"
(cd "$MNT/dev" && ./MAKEDEV -s all) || fail "MAKEDEV failed"

# MAKEDEV can finish successfully even if an individual mknod fails.
# /dev/console must exist before the installed kernel can start init.
rm -f "$MNT/dev/console" || fail "could not replace /dev/console"
mknod "$MNT/dev/console" c 4 0 || fail "could not create /dev/console"
chmod 0600 "$MNT/dev/console" || fail "could not set /dev/console permissions"
[ -c "$MNT/dev/console" ] || fail "installed /dev/console is not a character device"
# The GPT installer uses p0 (ESP), p1 (root), p2 (optional /home),
# and p3 (optional /usr). Make sure MAKEDEV created the static nodes
# required by the installed fstab before reporting a successful install.
for device in c0d0p0 c0d0p1 c0d0p2 c0d0p3; do
	[ -b "$MNT/dev/$device" ] || fail "MAKEDEV did not create /dev/$device"
done
if [ -n "$KEYMAP" ] && [ -s "/usr/lib/keymaps/$KEYMAP.map" ]; then
	cp -p "/usr/lib/keymaps/$KEYMAP.map" "$MNT/etc/keymap"
fi
mkdir -p "$MNT/usr/adm" "$MNT/tmp"
if [ -c /dev/random ]; then dd if=/dev/random of="$MNT/usr/adm/random.dat" bs=1024 count=1 || :; fi

echo
echo "Step 7: Configure networking"
if [ -x /bin/netconf ]; then
	/bin/netconf -p "$MNT" || echo "Network configuration was skipped; configure it after first boot."
else
	echo "netconf is unavailable; configure networking after first boot."
fi

echo
echo "Step 8: Install pkgin (when package archives are on the media)"
PKG_VERSION=$(uname -v | cut -f2 -d' ')
PKG_ARCH=$(uname -p)
PKG_DIR=/packages/$PKG_VERSION/$PKG_ARCH/All
if [ -d "$PKG_DIR" ]; then
	copied=yes
	for package in "$PKG_DIR"/pkgin-* "$PKG_DIR"/pkg_install-* "$PKG_DIR"/libarchive-*; do
		[ -f "$package" ] || continue
		cp -p "$package" "$MNT/tmp/" || copied=no
	done
	if [ "$copied" = yes ] && command -v chroot >/dev/null 2>&1; then
		chroot "$MNT" /bin/sh -c "pkg_add /tmp/pkgin-*" || echo "pkgin installation failed; install it later."
		rm -f "$MNT"/tmp/pkgin-* "$MNT"/tmp/pkg_install-* "$MNT"/tmp/libarchive-*
	fi
else
	echo "Package archives were not found; skipping pkgin."
fi

CD_DEVICE=$(mount 2>/dev/null | awk '$2 == "on" && $3 == "/" {print $1; exit}')
if [ -n "$CD_DEVICE" ]; then
	mkdir -p "$MNT/usr/etc"
	echo "cddrive=$CD_DEVICE" >> "$MNT/usr/etc/rc.package"
fi
sync
[ "$MOUNTED_HOME" != yes ] || { umount "$MNT/home" || fail "could not unmount /home"; MOUNTED_HOME=no; }
umount "$MNT/usr" || fail "could not unmount /usr"
MOUNTED_USR=no
umount "$MNT" || fail "could not unmount root"
MOUNTED_ROOT=no
sync

echo
echo "MINIX 3.4.0 amd64 has been installed on $DISKDEV."
echo "Remove the installation media and reboot in UEFI mode."
exit 0
