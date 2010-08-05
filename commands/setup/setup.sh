#!/bin/sh
#
#	setup 4.1 - install a MINIX distribution	
#
# Changes:
#    Aug     2005   robustness checks and beautifications  (Jorrit N. Herder)
#    Jul     2005   extended with autopart and networking  (Ben Gras)
#    Dec 20, 1994   created  (Kees J. Bot)
#						

LOCALRC=/usr/etc/rc.local
MYLOCALRC=/mnt/etc/rc.local
ROOTMB=64
ROOTSECTS="`expr $ROOTMB '*' 1024 '*' 2`"
USRKBFILE=/.usrkb
if [ ! -f "$USRKBFILE" ]
then	echo "Are you really running from CD?"
	echo "No $USRKBFILE file."
	exit 1
fi
USRKB="`cat /.usrkb`"
TOTALMB="`expr 3 + $USRKB / 1024 + $ROOTMB`"
ROOTFILES="`cat /.rootfiles`"
USRFILES="`cat /.usrfiles`"

if [ "$TOTALMB" -lt 1 ]
then	 
	echo "Are you really running from CD?"
	echo "Something wrong with size estimate on CD."
	exit 1
fi

if [ "$ROOTFILES" -lt 1 ]
then	 
	echo "Are you really running from CD?"
	echo "Something wrong with root files count on CD."
	exit 1
fi

if [ "$USRFILES" -lt 1 ]
then	 
	echo "Are you really running from CD?"
	echo "Something wrong with usr files count on CD."
	exit 1
fi

PATH=/bin:/sbin:/usr/bin
export PATH


usage()
{
    cat >&2 <<'EOF'
Usage:	setup		# Install a skeleton system on the hard disk.
	setup /usr	# Install the rest of the system (binaries or sources).

	# To install from other things then floppies:

	urlget http://... | setup /usr		# Read from a web site.
	urlget ftp://... | setup /usr		# Read from an FTP site.
	mtools copy c0d0p0:... - | setup /usr	# Read from the C: drive.
	dosread c0d0p0 ... | setup /usr		# Likewise if no mtools.
EOF
    exit 1
}

warn() 
{
  echo -e "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b ! $1"
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
umount /dev/$root 2>/dev/null && echo "Unmounted $root for you."
umount /dev/$home 2>/dev/null && echo "Unmounted $home for you."
umount /dev/$usr 2>/dev/null && echo "Unmounted $usr for you."

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

TMPMP=/m
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
		echo "Root partition size `expr $ROOTSECTS / 2`kb differs from default `expr $ROOTSECTSDEFAULT / 2`kb."
		echo "This is not a problem, but you may want to do a fresh install at some point to"
		echo "be able to benefit from the new default."
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

echo "
You have selected to (re)install MINIX 3 in the partition /dev/$primary.
The following subpartitions are now being created on /dev/$primary:

    Root subpartition:	/dev/$root	$ROOTMB MB
    /home subpartition:	/dev/$home	$homemb
    /usr subpartition:	/dev/$usr	rest of $primary
"
					# Secondary master bootstrap.
installboot -m /dev/$primary /usr/mdec/masterboot >/dev/null || exit
					# Partition the primary.
partition /dev/$primary 1 81:${ROOTSECTS}* 81:$homesize 81:0+ > /dev/null || exit

echo "Creating /dev/$root for / .."
mkfs.mfs /dev/$root || exit

if [ "$nohome" = 0 ]
then
	if [ ! "$auto" = r ]
	then	echo "Creating /dev/$home for /home .."
		mkfs.mfs -B $blocksizebytes /dev/$home || exit
	fi
else	echo "Skipping /home"
fi

echo "Creating /dev/$usr for /usr .."
mkfs.mfs -B $blocksizebytes /dev/$usr || exit

if [ "$nohome" = 0 ]
then
	fshome="home=/dev/$home"
else	fshome=""
fi

echo ""
echo " --- Step 7: Wait for files to be copied -------------------------------"
echo ""
echo "All files will now be copied to your hard disk. This may take a while."
echo ""

mount /dev/$usr /mnt >/dev/null || exit		# Mount the intended /usr.

(cd /usr || exit 1
 list="`ls | fgrep -v install`"
 for d in $list
 do	
 	cpdir -v $d /mnt/$d
 done
) | progressbar "$USRFILES" || exit	# Copy the usr floppy.

umount /dev/$usr >/dev/null || exit		# Unmount the intended /usr.
mount /dev/$root /mnt >/dev/null || exit

# Running from the installation CD.
cpdir -vx / /mnt | progressbar "$ROOTFILES" || exit	
cp /mnt/etc/motd.install /mnt/etc/motd

# Fix /var/log
rm /mnt/var/log
ln -s /usr/log /mnt/var/log

# CD remnants that aren't for the installed system
rm /mnt/etc/issue /mnt/CD /mnt/.* 2>/dev/null
echo >/mnt/etc/fstab "\
# Poor man's File System Table.

root=/dev/$root
usr=/dev/$usr
$fshome"

					# National keyboard map.
test -n "$keymap" && cp -p "/usr/lib/keymaps/$keymap.map" /mnt/etc/keymap

umount /dev/$root >/dev/null || exit 	# Unmount the new root.
mount /dev/$usr /mnt >/dev/null || exit

# Make bootable.
installboot -d /dev/$root /usr/mdec/bootblock /boot/boot >/dev/null || exit

edparams /dev/$root "rootdev=$root; ramimagedev=$root; minix(1,Start MINIX 3) { image=/boot/image_big; boot; }; newminix(2,Start Custom MINIX 3) { unset image; boot }; main() { echo By default, MINIX 3 will automatically load in 3 seconds.; echo Press ESC to enter the monitor for special configuration.; trap 3000 boot; menu; }; save" || exit
pfile="/mnt/src/tools/fdbootparams"
echo "rootdev=$root; ramimagedev=$root; save" >$pfile
# Save name of CD drive
cddrive="`mount | grep usr | awk '{ print $1 }' | sed 's/p.*//'`" 
echo "cddrive=$cddrive" >>/mnt/etc/rc.package

bios="`echo $primary | sed -e 's/d./dX/g' -e 's/c.//g'`"

if [ ! "$auto" = "r" ]
then	if mount /dev/$home /home 2>/dev/null
	then	for u in bin ast
		do	h=`eval echo "~$u"`
			if mkdir $h
			then	echo " * Creating home directory for $u in $h"
				cpdir /usr/ast $h
				chown -R $u:operator $h
			else	echo " * Couldn't create $h"
			fi
		done
		umount /dev/$home
	fi
fi

echo "Saving random data.."
dd if=/dev/random of=/mnt/adm/random.dat bs=1024 count=1

umount /dev/$usr >/dev/null || exit

# Networking.
echo ""
echo " --- Step 8: Select your Ethernet chip ---------------------------------"
echo ""

mount /dev/$root /mnt >/dev/null || exit
mount /dev/$usr /mnt/usr >/dev/null || exit

/bin/netconf -p /mnt || echo FAILED TO CONFIGURE NETWORK

umount /dev/$usr && echo Unmounted $usr
umount /dev/$root && echo Unmounted $root

echo "
Please type 'shutdown' to exit MINIX 3 and enter the boot monitor. At
the boot monitor prompt, type 'boot $bios', where X is the bios drive
number of the drive you installed on, to try your new MINIX system.
$biosdrivename

This ends the MINIX 3 setup script.  After booting your newly set up system,
you can run the test suites as indicated in the setup manual.  You also 
may want to take care of local configuration, such as securing your system
with a password.  Please consult the usage manual for more information. 

"

