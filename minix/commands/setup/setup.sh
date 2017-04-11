#!/bin/sh
#
#	setup 4.1 - install a MINIX distribution	
#
# Changes:
#    Aug     2005   robustness checks and beautifications  (Jorrit N. Herder)
#    Jul     2005   extended with autopart and networking  (Ben Gras)
#    Dec 20, 1994   created  (Kees J. Bot)
#						

ROOTMB=128
ROOTSECTS="`expr $ROOTMB '*' 1024 '*' 2`"
BOOTXXSECTS=32
#LSC: Slight over shoot, as files for / are taken into account, although
#     metadata is not, all in all should be pretty accurate.
USRKB=$(cat /i386/binary/sets/*.size | awk '{s+=$1} END{print (s/1024)}')
TOTALMB="`expr 3 + $USRKB / 1024 + $ROOTMB`"
TOTALFILES="`find -x / | wc -l`"

# /usr/install isn't copied onto the new system; compensate
INSTALLDIR=/usr/install
if [ -d $INSTALLDIR ]
then	USRFILES=$(($USRFILES - `find -x $INSTALLDIR | wc -l`))
	USRKB=$(($USRKB - `du -sxk $INSTALLDIR | awk '{ print $1 }'`))
fi

if [ -z "$FSTYPE" ]
then	FSTYPE=mfs
fi

PATH=/bin:/sbin:/usr/bin:/usr/sbin
export PATH


usage()
{
    cat >&2 <<'EOF'
Usage:	setup		# Install a skeleton system on the hard disk.
	setup /usr	# Install the rest of the system (binaries or sources).

	# To install from other things then floppies:

	fetch -q -o - http://... | setup /usr	# Read from a web site.
	fetch -q -o - ftp://... | setup /usr	# Read from an FTP site.
	mtools copy c0d0p0:... - | setup /usr	# Read from the C: drive.
	dosread c0d0p0 ... | setup /usr		# Likewise if no mtools.
EOF
    exit 1
}

warn() 
{
  echo -e "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b ! $1"
}

check_mbr()
{
	# check for potential problems with old mbr.
	disk=`echo -n "/dev/$primary" | sed 's/p[0-3]//'`
	minix_primaries=`echo -n "" | fdisk "$disk" | grep "MINIX" | wc -l`
	if [ "$minix_primaries" -gt 1 ]
	then
		# old mbr + bootxx will not work with several partitions of
		# the same type.
		dd if=/usr/mdec/mbr of=temp_mbr_netbsd bs=1 count=440 2>/dev/null
		dd if="$disk" bs=1 count=440 2>/dev/null | cmp - temp_mbr_netbsd >/dev/null
		if [ "$?" -ne 0 ]
		then
			echo ""
			echo "Warning: you have MBR which doesn't support multiple MINIX 3 partitions!"
			echo "You will be able to boot from the first one only!"
			echo -n "Do you want to install new MBR into $disk? [Y] "
			read ok
			if [ "$ok" = Y -o "$ok" = y -o "$ok" = "" ]
			then
				installboot_nbsd -m "$disk" /usr/mdec/mbr >/dev/null
			fi
		fi
		rm temp_mbr_netbsd
	fi

}

# No options.
while getopts '' opt; do usage; done
shift `expr $OPTIND - 1`

if [ "$USER" != root ]
then	echo "Please run setup as root."
	exit 1
fi

# Find out what we are running from.
exec 9<&0 </etc/mtab			# Mounted file table.
read thisroot rest			# Current root (/dev/ram or /dev/fd?)
read fdusr rest				# USR (/dev/fd? or /dev/fd?p2)
exec 0<&9 9<&-

# What do we know about ROOT?
case $thisroot:$fdusr in
/dev/ram:/dev/fd0p2)	fdroot=/dev/fd0		# Combined ROOT+USR in drive 0
			;;
/dev/ram:/dev/fd1p2)	fdroot=/dev/fd1		# Combined ROOT+USR in drive 1
			;;
/dev/ram:/dev/fd*)	fdroot=unknown		# ROOT is some other floppy
			;;
/dev/fd*:/dev/fd*)	fdroot=$thisroot	# ROOT is mounted directly
			;;
*)			fdroot=$thisroot	# ?
esac

echo -n "
Welcome to the MINIX 3 setup script.  This script will guide you in setting up
MINIX on your machine.  Please consult the manual for detailed instructions.

Note 1: If the screen blanks, hit CTRL+F3 to select \"software scrolling\".
Note 2: If things go wrong then hit CTRL+C to abort and start over.
Note 3: Default answers, like [y], can simply be chosen by hitting ENTER.
Note 4: If you see a colon (:) then you should hit ENTER to continue.
:"
read ret

# begin Step 1
echo ""
echo " --- Step 1: Select keyboard type --------------------------------------"
echo ""

    echo "What type of keyboard do you have?  You can choose one of:"
    echo ""
    ls -C /usr/lib/keymaps | sed -e 's/\.map//g' -e 's/^/    /'
    echo ""
step1=""
while [ "$step1" != ok ]
do
    echo -n "Keyboard type? [us-std] "; read keymap
    test -n "$keymap" || keymap=us-std
    if loadkeys "/usr/lib/keymaps/$keymap.map" 2>/dev/null 
    then step1=ok 
    else warn "invalid keyboard"
    fi
done
# end Step 1

# begin Step 2
#step2=""
#while [ "$step2" != ok ]
#do
#	echo ""
#	echo " --- Step 2: Select minimal or full distribution -----------------------"
#	echo ""
#	echo "You can install MINIX as (M)inimal or (F)ull. (M)inimal"
#	echo "includes only the binary system and basic system sources."
#	echo "(F)ull also includes commands sources."
#	echo ""
#	echo "Please select:"
#	echo "  (M)inimal install (only basic sources) ($NOSRCMB MB required)"
#	echo "  (F)ull install (full install) ($TOTALMB MB required)"
#	echo " "
#	echo -n "Basic (M)inimal or (F)ull install? [F] "
#	read conf
#	case "$conf" in
#	"") 	step2="ok"; nobigsource="" ;;
#	[Ff]*)	step2="ok"; nobigsource="" ;;
#	[Mm]*)	step2="ok"; nobigsource="1"; TOTALMB=$NOSRCMB; USRFILES=$NOSRCUSRFILES ;;
#	esac
#done
# end Step 2

echo ""
echo " --- Step 2: Selecting full distribution -------------------------------"
echo ""
nobigsource=""

# begin Step 3
step3=""
while [ "$step3" != ok ]
do
	echo ""
	echo " --- Step 3: Create or select a partition for MINIX 3 -------------------"
	echo ""

    echo "Now you need to create a MINIX 3 partition on your hard disk."
    echo "You can also select one that's already there."
    echo " "
    echo "If you have an existing installation, reinstalling will let you"
    echo "keep your current partitioning and subpartitioning, and overwrite"
    echo "everything except your s1 subpartition (/home). If you want to"
    echo "reinstall, select your existing minix partition."
    echo " "
    echo "Unless you are an expert, you are advised to use the automated"
    echo "step-by-step help in setting up."
    echo ""
    ok=""
    while [ "$ok" = "" ]
    do
	    echo -n "Press ENTER for automatic mode, or type 'expert': "
	    read mode
	    if [ -z "$mode" ]; then auto="1"; ok="yes"; fi 
	    if [ "$mode" = expert ]; then auto=""; ok="yes"; fi
	    if [ "$ok" != yes ]; then warn "try again"; fi 
    done

	primary=

	if [ -z "$auto" ]
	then
		# Expert mode
		echo -n "
MINIX needs one primary partition of $TOTALMB MB for a full install,
plus what you want for /home.

If there is no free space on your disk then you have to choose an option:
   (1) Delete one or more partitions
   (2) Allocate an existing partition to MINIX 3
   (3) Exit setup and shrink a partition using a different OS

To make this partition you will be put in the editor \"part\".  Follow the
advice under the '!' key to make a new partition of type MINIX.  Do not
touch an existing partition unless you know precisely what you are doing!
Please note the name of the partition (e.g. c0d0p1, c0d1p3, c1d1p0) you
make.  (See the devices section in usage(8) on MINIX device names.)
:"
		read ret

		while [ -z "$primary" ]
		do
		    part || exit

		    echo -n "
Please finish the name of the primary partition you have created:
(Just type ENTER if you want to rerun \"part\")                   /dev/"
		    read primary
		done
		echo ""
		echo "This is the point of no return.  You have selected to install MINIX"
		echo "on partition /dev/$primary.  Please confirm that you want to use this"
		echo "selection to install MINIX."
		echo ""
		confirmation=""

		if [ ! -b "/dev/$primary" ]
		then	echo "/dev/$primary is not a block device."
		fi

		while [ -z "$confirmation" -o "$confirmation" != yes -a "$confirmation" != no ]
		do
			echo -n "Are you sure you want to continue? Please enter 'yes' or 'no': "
			read confirmation
			if [ "$confirmation" = yes ]; then step3=ok; fi
		done
		biosdrivename="Actual BIOS device name unknown, due to expert mode."
	else
		if [ "$auto" = "1" ]
		then
			# Automatic mode
			PF="/tmp/pf"
			if autopart -m$TOTALMB -f$PF
			then	if [ -s "$PF" ]
				then
					set `cat $PF`
					bd="$1"
					bdn="$2"
					biosdrivename="Probably, the right command is \"boot $bdn\"."
					if [ -b "/dev/$bd" ]
					then	primary="$bd"
					else	echo "Funny device $bd from autopart."
					fi
				else
					echo "Didn't find output from autopart."
				fi 
			else	echo "Autopart tool failed. Trying again."
			fi

			# Reset at retries and timeouts in case autopart left
			# them messy.
			atnormalize

			if [ -n "$primary" ]; then step3=ok; fi
		fi
	fi

	if [ ! -b "/dev/$primary" ]
	then	echo Doing step 3 again.
		step3=""
	else
		devsize="`devsize /dev/$primary`"

		if [ "$devsize" -lt 1 ]
		then	echo "/dev/$primary is a 0-sized device."
			step3=""
		fi
	fi
done	# while step3 != ok
# end Step 3

root=${primary}s0
home=${primary}s1
usr=${primary}s2
umount /dev/$home 2>/dev/null && echo "Unmounted $home for you."
umount /dev/$usr 2>/dev/null && echo "Unmounted $usr for you."
umount /dev/$root 2>/dev/null && echo "Unmounted $root for you."

devsizemb="`expr $devsize / 1024 / 2`"
maxhome="`expr $devsizemb - $TOTALMB - 1`"

if [ "$devsizemb" -lt "$TOTALMB" ]
then	echo "The selected partition ($devsizemb MB) is too small."
	echo "You'll need $TOTALMB MB at least."
	exit 1
fi

if [ "$maxhome" -lt 1 ]
then	echo "Note: you can't have /home with that size partition."
	maxhome=0
fi

TMPMP=/mnt
mkdir $TMPMP >/dev/null 2>&1

confirm=""

while [ "$confirm" = "" ]
do
	auto=""
	echo ""
echo " --- Step 4: Reinstall choice ------------------------------------------"
	if mount -r /dev/$home $TMPMP >/dev/null 2>&1
	then	umount /dev/$home >/dev/null 2>&1
		echo ""
		echo "You have selected an existing MINIX 3 partition."
		echo "Type F for full installation (to overwrite entire partition)"
		echo "Type R for a reinstallation (existing /home will not be affected)"
		echo ""
		echo -n "(F)ull or (R)einstall? [R] "
		read conf
		case "$conf" in
		"") 	confirm="ok"; auto="r"; ;;
		[Rr]*)	confirm="ok"; auto="r"; ;;
		[Ff]*)	confirm="ok"; auto="" ;;
		esac

	else	echo ""
		echo "No old /home found. Doing full install."
		echo ""
		confirm="ok";
	fi

done

rmdir $TMPMP

nohome="0"

homesize=""
if [ ! "$auto" = r ]
then	
echo ""
echo " --- Step 5: Select the size of /home ----------------------------------"
	while [ -z "$homesize" ]
	do

		# 20% of what is left over after / and /usr
		# are taken.
		defmb="`expr $maxhome / 5`"
		if [ "$defmb" -gt "$maxhome" ]
		then
			defmb=$maxhome
		fi

		echo ""
		echo "MINIX will take up $TOTALMB MB, without /home."
		echo -n "How big do you want your /home to be in MB (0-$maxhome) ? [$defmb] "
		read homesize
		if [ "$homesize" = "" ] ; then homesize=$defmb; fi
		if [ "$homesize" -lt 1 ]
		then	nohome=1
			echo "Ok, not making a /home."
			homesize=0
		else
			if [ "$homesize" -gt "$maxhome" ]
			then	echo "That won't fit!"
				homesize=""
			else
				echo ""
				echo -n "$homesize MB Ok? [Y] "
				read ok
				[ "$ok" = Y -o "$ok" = y -o "$ok" = "" ] || homesize=""
			fi
		fi
		echo ""
	done
	# Homesize in sectors
	homemb="$homesize MB"
	homesize="`expr $homesize '*' 1024 '*' 2`"
else
	# Root size same as our default? If not, warn and keep old root size
	ROOTSECTSDEFAULT=$ROOTSECTS
	ROOTSECTS="`devsize /dev/$root`"
	ROOTMB="`expr $ROOTSECTS / 2048`"
	if [ $ROOTSECTS -ne $ROOTSECTSDEFAULT ]
	then
		# Check if we 
		echo "Root partition size `expr $ROOTSECTS / 2`kb differs from default `expr $ROOTSECTSDEFAULT / 2`kb."
		echo "This is not a problem, but you may want to do a fresh install at some point to"
		echo "be able to benefit from the new default."
	fi

	# Check if enough space for new boot (even if old used)
	bootspace=$((`devsize /dev/$primary`-`devsize /dev/$root`-`devsize /dev/$home`-`devsize /dev/$usr`)) >/dev/null
	if [ $bootspace -lt $BOOTXXSECTS ]
	then
		echo "Root partition size will be reduced by up to 16Kb to fit new bootloader."
		echo "This is not a problem."
		ROOTSECTS=`expr $ROOTSECTS - $BOOTXXSECTS + $bootspace`
	fi

	# Recompute totals based on root size
	TOTALMB="`expr 3 + $USRKB / 1024 + $ROOTMB`"
	maxhome="`expr $devsizemb - $TOTALMB - 1`"

	homepart="`devsize /dev/$home`"
	homesize="`expr $homepart / 2 / 1024`"
	if [ "$homesize" -gt "$maxhome" ]
	then
		echo "Sorry, but your /home is too big ($homesize MB) to leave enough"
		echo "space on the rest of the partition ($devsizemb MB) for your"
		echo "selected installation size ($TOTALMB MB)."
		exit 1
	fi
	# Homesize unchanged (reinstall)
	homesize=exist
	homemb="current size"
fi

minblocksize=1
maxblocksize=64
blockdefault=4

if [ ! "$auto" = "r" ]
then
	echo ""
echo " --- Step 6: Select a block size ---------------------------------------"
	echo ""
	
	echo "The default file system block size is $blockdefault kB."
	echo ""
	
	while [ -z "$blocksize" ]
	do	
		echo -n "Block size in kilobytes? [$blockdefault] "; read blocksize
		test -z "$blocksize" && blocksize=$blockdefault
		if [ "$blocksize" -lt $minblocksize -o "$blocksize" -gt $maxblocksize ]
		then	
			warn "At least $minblocksize kB and at most $maxblocksize kB please."
			blocksize=""
		fi
	done
else
	blocksize=$blockdefault
fi

blocksizebytes="`expr $blocksize '*' 1024`"

bootsectors=$BOOTXXSECTS

check_mbr

echo "
You have selected to (re)install MINIX 3 in the partition /dev/$primary.
The following subpartitions are now being created on /dev/$primary:

    Root subpartition:	/dev/$root	$ROOTMB MB
    /home subpartition:	/dev/$home	$homemb
    /usr subpartition:	/dev/$usr	rest of $primary
"
					# Secondary master bootstrap.
# New boot doesn't require mbr on pN (bootxx will be there)
# When necessarily mbr is installed on dN by partition.
					# Partition the primary.
partition /dev/$primary $bootsectors 81:${ROOTSECTS}* 81:$homesize 81:0+ > /dev/null || exit

echo "Creating /dev/$root for / .."
mkfs.mfs /dev/$root || exit

if [ "$nohome" = 0 ]
then
	if [ ! "$auto" = r ]
	then	echo "Creating /dev/$home for /home .."
		mkfs.$FSTYPE -B $blocksizebytes /dev/$home || exit
	fi
else	echo "Skipping /home"
fi

echo "Creating /dev/$usr for /usr .."
mkfs.$FSTYPE -B $blocksizebytes /dev/$usr || exit

if [ "$nohome" = 0 ]
then
	fshome="/dev/$home	/home	$FSTYPE	rw			0	2"
else	fshome=""
fi

echo ""
echo " --- Step 7: Wait for files to be copied -------------------------------"
echo ""
echo "All files will now be copied to your hard disk. This may take a while."
echo ""

mount /dev/$root /mnt >/dev/null || exit
mkdir -p /mnt/usr
mount /dev/$usr /mnt/usr >/dev/null || exit		# Mount the intended /usr.
if [ "$nohome" = 0 ]; then
	mkdir -p /mnt/home
	mount /dev/$home /mnt/home >/dev/null || exit		# Mount the intended /home
fi

# Running from the installation CD.
for set in /i386/binary/sets/*.tgz; do
	echo "Extracting $(basename "$set")..."
	COUNT_FILES=$(cat $(echo "$set" | sed -e "s/\.tgz/\.count/"))
	(cd /mnt; pax -rz -f $set -v -pe 2>&1 | progressbar "$COUNT_FILES" || exit)
done;

echo "Creating device nodes..."
(cd /mnt/dev; MAKEDEV -s all)

# Fix permissions
chmod $(stat -f %Lp /usr) /mnt/usr
chown $(stat -f %u /usr) /mnt/usr
chgrp $(stat -f %g /usr) /mnt/usr
if [ "$nohome" = 0 ]; then
	chmod $(stat -f %Lp /home) /mnt/home
	chown $(stat -f %u /home) /mnt/home
	chgrp $(stat -f %g /home) /mnt/home
fi

# CD remnants that aren't for the installed system
rm /mnt/etc/issue /mnt/CD /mnt/.* 2>/dev/null
echo >/mnt/etc/fstab "/dev/$root	/		mfs	rw			0	1
/dev/$usr	/usr		$FSTYPE	rw			0	2
$fshome
none		/sys		devman	rw,rslabel=devman	0	0
none		/dev/pts	ptyfs	rw,rslabel=ptyfs	0	0"

					# National keyboard map.
test -n "$keymap" && cp -p "/usr/lib/keymaps/$keymap.map" /mnt/etc/keymap

# XXX we have to use "-f" here, because installboot worries about BPB, which
# we don't have...
installboot_nbsd -f /dev/$primary /usr/mdec/bootxx_minixfs3 >/dev/null || exit
# give the install the boot loader
cp /usr/mdec/boot_monitor /mnt/
minixdir=/mnt/boot/minix_default
if [ ! -f $minixdir/kernel ]
then	rm -rf $minixdir
	cp -r /mnt/boot/minix/.temp $minixdir
fi
if [ ! -e /mnt/boot/minix_latest ]
then	ln -sf minix_default /mnt/boot/minix_latest
fi
chroot /mnt update_bootcfg

bios="`echo $primary | sed -e 's/d./dX/g' -e 's/c.//g'`"

echo "Saving random data.."
dd if=/dev/random of=/mnt/usr/adm/random.dat bs=1024 count=1

# Networking.
echo ""
echo " --- Step 8: Select your Ethernet chip ---------------------------------"
echo ""

/bin/netconf -p /mnt || echo FAILED TO CONFIGURE NETWORK

PACKAGES_DIR="/packages/$(uname -v | cut -f2 -d' ')/$(uname -p)/All"
if [ -e "$PACKAGES_DIR" ]
then
	echo "Installing pkgin..."
	sh -c "cp $PACKAGES_DIR/pkgin-* $PACKAGES_DIR/pkg_install-* $PACKAGES_DIR/libarchive-* /mnt/tmp &&
	 chroot /mnt pkg_add /tmp/pkgin-*"
	rm -f /mnt/tmp/*

	# Save name of CD drive
	cddrive=$(mount | grep ' on / ' | cut -f1 -d' ')
	echo "cddrive=$cddrive" >>/mnt/usr/etc/rc.package
else
	echo "Package dir not found, skipping pkgin installation..."
fi

if [ "$nohome" = 0 ]; then
	umount /dev/$home && echo Unmounted $home
fi
umount /dev/$usr && echo Unmounted $usr
umount /dev/$root && echo Unmounted $root

echo "
Please type 'shutdown -r now' to exit MINIX 3 and reboot. To boot into
your new system, you might have to remove the installation media.

This ends the MINIX 3 setup script.  You may want to take care of post
installation steps, such as local testing and configuration.

Please consult the user manual for more information.

"

