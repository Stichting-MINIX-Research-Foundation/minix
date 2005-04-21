#!/bin/sh
#
#	setup 4.1 - install a Minix distribution	Author: Kees J. Bot
#								20 Dec 1994

PATH=/bin:/usr/bin
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

# No options.
while getopts '' opt; do usage; done
shift `expr $OPTIND - 1`

# Installing a floppy set?
case $# in
0)  # No, we're installing a skeleton system on the hard disk.
    ;;
1)
    cd "$1" || exit

    # Annoying message still there?
    grep "'setup /usr'" /etc/issue >/dev/null 2>&1 && rm -f /etc/issue

    if [ -t 0 ]
    then
	size=bad
	while [ "$size" = bad ]
	do
	    echo -n "\
What is the size of the images on the diskettes? [all] "; read size

	    case $size in
	    ''|360|720|1200|1440)
		;;
	    *)	echo "Sorry, I don't believe \"$size\", try again." >&2
		size=bad
	    esac
	done

	drive=
	while [ -z "$drive" ]
	do
	    echo -n "What floppy drive to use? [0] "; read drive

	    case $drive in
	    '')	drive=0
		;;
	    [01])
		;;
	    *)	echo "It must be 0 or 1, not \"$drive\"."
		drive=
	    esac
	done

	vol -r $size /dev/fd$drive | uncompress | tar xvfp -
    else
	# Standard input is where we can get our files from.
	uncompress | tar xvfp -
    fi

    echo Done.
    exit
    ;;
*)
    usage
esac

# Installing Minix on the hard disk.
# Must be in / or we can't mount or umount.
case "`pwd`" in
/?*)
    echo "Please type 'cd /' first, you are locking up `pwd`" >&2	
    exit 1
esac
case "$0" in
/tmp/*)
    rm -f "$0"
    ;;
*)  cp -p "$0" /tmp/setup
    exec /tmp/setup
esac

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
    echo -n "\
It looks like Minix has been installed on disk already.  Are you sure you
know what you are doing? [n] "
    read yn
    case "$yn" in
    [yY]*|sure)	;;
    *)	exit
    esac
esac

echo -n "\
This is the Minix installation script.

Note 1: If the screen blanks suddenly then hit F3 to select \"software
	scrolling\".

Note 2: If things go wrong then hit DEL and start over.

Note 3: The installation procedure is described in the manual page
	usage(8).  It will be hard without it.

Note 4: Some questions have default answers, like this: [y]
	Simply hit RETURN (or ENTER) if you want to choose that answer.

Note 5: If you see a colon (:) then you should hit RETURN to continue.
:"
read ret

echo "
What type of keyboard do you have?  You can choose one of:
"
ls -C /usr/lib/keymaps | sed -e 's/\.map//g' -e 's/^/    /'
echo -n "
Keyboard type? [us-std] "; read keymap
test -n "$keymap" && loadkeys "/usr/lib/keymaps/$keymap.map"

echo -n "
Minix needs one primary partition of at least 35 Mb.  (It fits in 25 Mb, but
it needs 35 Mb if fully recompiled.  Add more space to taste, but don't
overdo it, there are limits to the size of a file system.)

If there is no free space on your disk then you have to back up one of the
other partitions, shrink, and reinstall.  See the appropriate manuals of the
the operating systems currently installed.  Restart your Minix installation
after you have made space.

To make this partition you will be put in the editor \"part\".  Follow the
advice under the '!' key to make a new partition of type MINIX.  Do not
touch an existing partition unless you know precisely what you are doing!
Please note the name of the partition (e.g. c0d0p1, c0d1p3, c1d1p0) you
make.  (See the devices section in usage(8) on Minix device names.)
:"
read ret

primary=
while [ -z "$primary" ]
do
    part || exit

    echo -n "
Please finish the name of the primary partition you have created:
(Just type RETURN if you want to rerun \"part\")                   /dev/"
    read primary
done

root=${primary}s0
swap=${primary}s1
usr=${primary}s2

hex2int()
{
    # Translate hexadecimal to integer.
    local h d i

    h=$1
    i=0
    while [ -n "$h" ]
    do
	d=$(expr $h : '\(.\)')
	h=$(expr $h : '.\(.*\)')
	d=$(expr \( 0123456789ABCDEF : ".*$d" \) - 1)
	i=$(expr $i \* 16 + $d)
    done
    echo $i
}

# Compute the amount of memory available to Minix.
memsize=0
ifs="$IFS"
IFS=','
set -- $(sysenv memory)
IFS="$ifs"

for mem
do
    mem=$(expr $mem : '.*:\(.*\)')
    memsize=$(expr $memsize + $(hex2int $mem) / 1024)
done

# Compute an advised swap size.
swapadv=0
case `arch` in
i86)
    test $memsize -lt 4096 && swapadv=$(expr 4096 - $memsize)
    ;;
*)  test $memsize -lt 6144 && swapadv=$(expr 6144 - $memsize)
esac

echo -n "
How much swap space would you like?  Swapspace is only needed if this
system is memory starved, like a 16-bit system with less then 2M, or a
32-bit system with less then 4M.  Minix swapping isn't very good yet, so
there is no need for it otherwise.
		    Size in kilobytes? [$swapadv] "
swapsize=
read swapsize
test -z "$swapsize" && swapsize=$swapadv

echo -n "
You have created a partition named:	/dev/$primary
The following subpartitions are about to be created on /dev/$primary:

    Root subpartition:	/dev/$root	1440 kb
    Swap subpartition:	/dev/$swap	$swapsize kb
    /usr subpartition:	/dev/$usr	rest of $primary

Hit return if everything looks fine, or hit DEL to bail out if you want to
think it over.  The next step will destroy /dev/$primary.
:"
read ret
					# Secondary master bootstrap.
installboot -m /dev/$primary /usr/mdec/masterboot >/dev/null || exit

					# Partition the primary.
p3=0:0
test "$swapsize" -gt 0 && p3=81:`expr $swapsize \* 2`
partition /dev/$primary 1 81:2880* $p3 81:0+ >/dev/null || exit

if [ "$swapsize" -gt 0 ]
then
    # We must have that swap, now!
    mkswap -f /dev/$swap || exit
    mount -s /dev/$swap || exit
else
    # Forget about swap.
    swap=
fi

echo "
Migrating from floppy to disk...
"

mkfs /dev/$usr
echo "\
Scanning /dev/$usr for bad blocks.  (Hit DEL to stop the scan if are
absolutely sure that there can not be any bad blocks.  Otherwise just wait.)"
trap ': nothing' 2
readall -b /dev/$usr | sh
echo "Scan done"
sleep 2
trap 2

mount /dev/$usr /mnt || exit		# Mount the intended /usr.

cpdir -v /usr /mnt || exit		# Copy the usr floppy.

umount /dev/$usr || exit		# Unmount the intended /usr.

umount $fdusr				# Unmount the /usr floppy.

mount /dev/$usr /usr || exit		# A new /usr

if [ $fdroot = unknown ]
then
    echo "
By now the floppy USR has been copied to /dev/$usr, and it is now in use as
/usr.  Please insert the installation ROOT floppy in a floppy drive."

    drive=
    while [ -z "$drive" ]
    do
	echo -n "What floppy drive is it in? [0] "; read drive

	case $drive in
	'')	drive=0
	    ;;
	[01])
	    ;;
	*)	echo "It must be 0 or 1, not \"$drive\"."
	    drive=
	esac
    done
    fdroot=/dev/fd$drive
fi

echo "
Copying $fdroot to /dev/$root
"

mkfs /dev/$root || exit
mount /dev/$root /mnt || exit
if [ $thisroot = /dev/ram ]
then
    # Running from the RAM disk, root image is on a floppy.
    mount $fdroot /root || exit
    cpdir -v /root /mnt || exit
    umount $fdroot || exit
    cpdir -f /dev /mnt/dev		# Copy any extra MAKEDEV'd devices
else
    # Running from the floppy itself.
    cpdir -vx / /mnt || exit
    chmod 555 /mnt/usr
fi

					# Change /etc/fstab.
echo >/mnt/etc/fstab "\
# Poor man's File System Table.

root=/dev/$root
${swap:+swap=/dev/$swap}
usr=/dev/$usr"

					# How to install further?
echo >/mnt/etc/issue "\
Login as root and run 'setup /usr' to install floppy sets."

					# National keyboard map.
test -n "$keymap" && cp -p "/usr/lib/keymaps/$keymap.map" /mnt/etc/keymap

umount /dev/$root || exit		# Unmount the new root.

# Compute size of the second level file block cache.
case `arch` in
i86)
    cache=`expr "0$memsize" - 1024`
    test $cache -lt 32 && cache=0
    test $cache -gt 512 && cache=512
    ;;
*)
    cache=`expr "0$memsize" - 2560`
    test $cache -lt 64 && cache=0
    test $cache -gt 1024 && cache=1024
esac
echo "Second level file system block cache set to $cache kb."
if [ $cache -eq 0 ]; then cache=; else cache="ramsize=$cache"; fi

					# Make bootable.
installboot -d /dev/$root /usr/mdec/bootblock /boot >/dev/null || exit
edparams /dev/$root "rootdev=$root; ramimagedev=$root; $cache; save" || exit

echo "
Please insert the installation ROOT floppy and type 'halt' to exit Minix.
You can type 'boot $primary' to try the newly installed Minix system.  See
\"TESTING\" in the usage manual."
