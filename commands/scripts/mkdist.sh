#!/bin/sh
#
#	mkdist 3.6 - Make a Minix distribution		Author: Kees J. Bot
#								20 Dec 1994

system=`uname`

usage()
{
    case $system in
    Minix)	echo "Usage: $0" >&2
	;;
    Minix-vmd)	echo "Usage: $0 base-path root-device usr-device" >&2
    esac
    exit 1
}

# No options.
while getopts '' opt; do usage; done
shift `expr $OPTIND - 1`

case $system:$# in
Minix:0)
    # Interactive.
    case "$0" in
    /tmp/*)
	rm -f "$0"
	;;
    *)  # Move out of /usr.
	cp -p "$0" /tmp/mkdist
	exec /tmp/mkdist
    esac
    std=t
    base=/
    export PATH=/bin:/usr/bin
    ;;
Minix-vmd:3)
    # Called by an external script from Minix-vmd to help make a distribution.
    std=
    base="$1" rootdev="$2" usrdev="$3"
esac

usrlist="
bin
bin/MAKEDEV
bin/arch
bin/badblocks
bin/chmod
bin/clone
bin/compress
bin/cp
bin/cpdir
bin/df
`test -f $base/usr/bin/mtools || echo bin/dosdir bin/dosread bin/doswrite`
`test -f $base/usr/bin/mtools && echo bin/mtools`
bin/edparams
bin/getty
bin/grep
bin/installboot
bin/isodir
bin/isoinfo
bin/isoread
bin/kill
bin/ln
bin/login
bin/ls
bin/mined
bin/mkdir
bin/mkfs
bin/mknod
bin/mkswap
bin/mv
bin/od
bin/part
bin/partition
bin/readall
bin/repartition
bin/rm
bin/rmdir
bin/sed
bin/setup
bin/shutdown
bin/sleep
bin/sort
bin/stty
bin/sysenv
bin/tar
bin/uname
bin/uncompress
bin/update
bin/vol
bin/zcat
etc
etc/rc
lib
lib/keymaps
`cd $base/usr && echo lib/keymaps/*`
lib/pwdauth
mdec
mdec/boot
mdec/bootblock
mdec/jumpboot
mdec/masterboot
tmp
"

if [ "$std" ]
then
    # Find the root device, and the real root device.
    . /etc/fstab
    realroot=`printroot -r`
    if [ $realroot = $root ]
    then
	rootdir=/
    else
	umount $root >/dev/null 2>&1
	mount $root /root || exit
	rootdir=/root
    fi

    echo -n "
The installation root and /usr can be put on either one diskette of at least
1.2 Mb, or on two diskettes of at least 720 kb.

Do you want to use a single diskette of at least 1.2 Mb? [y] "; read single

    case $single in
    ''|[yY]*|sure)
	single=t
	;;
    *)  single=
    esac

    echo -n "Which drive to use? [0] "; read drive

    case $drive in
    '') drive=0
	;;
    [01])	;;
    *)  echo "Please type '0' or '1'" >&2; exit 1
    esac

    if [ "$single" ]
    then
	echo -n "Insert the root+usr diskette in drive $drive and hit RETURN"
    else
	echo -n "Insert the root diskette in drive $drive and hit RETURN"
    fi
    read ret

    rootdev=/dev/fd$drive
    v1=-1
else
    rootdir=$base
    v1='-t 1'
fi

umount $rootdev 2>/dev/null
if [ "$std" ]
then
    umount ${rootdev}p1 2>/dev/null
    umount ${rootdev}p2 2>/dev/null
else
    umount $rootdir/minix 2>/dev/null
    umount $rootdir/etc 2>/dev/null
fi
mkfs $v1 -i 272 $rootdev 480 || exit
mount $rootdev /mnt || exit
if [ "$std" ]
then
    partition -mf $rootdev 0 81:960 81:240 81:240 >/dev/null || exit
    repartition $rootdev >/dev/null || exit
    mkfs $v1 ${rootdev}p1 || exit
    mkfs $v1 ${rootdev}p2 || exit
    mount ${rootdev}p1 $rootdir/minix || exit	# Hide /minix and /etc
    mount ${rootdev}p2 $rootdir/etc 2>/dev/null # (complains about /etc/mtab)
else
    install -d /tmp/.minix || exit
    install -d /tmp/.etc || exit		# Hide /minix and /etc
    mount -t lo /tmp/.minix $rootdir/minix || exit
    mount -t lo /tmp/.etc $rootdir/etc || exit
fi
cpdir -vx $rootdir /mnt || exit
install -d -o 0 -g 0 -m 755 /mnt || exit
install -d -o 0 -g 0 -m 555 /mnt/root || exit
install -d -o 0 -g 0 -m 555 /mnt/mnt || exit
install -d -o 0 -g 0 -m 555 /mnt/usr || exit
if [ "$std" ]
then
    umount ${rootdev}p2 2>/dev/null	# Unhide /etc
    umount ${rootdev}p1 || exit		# Unhide /minix
else
    umount $rootdir/etc || exit		# Unhide /etc
    umount $rootdir/minix || exit	# Unhide /minix
fi
install -d -o 2 -g 0 -m 755 /mnt/minix || exit
install -d -o 2 -g 0 -m 755 /mnt/etc || exit
set `ls -t $rootdir/minix`	# Install the latest kernel
install -c $rootdir/minix/$1 /mnt/minix/`echo $1 | sed 's/r[0-9]*$//` || exit
cpdir -v $base/usr/src/etc /mnt/etc || exit	# Install a fresh /etc
chown -R 0:0 /mnt/etc				# Patch up owner and mode
chmod 600 /mnt/etc/shadow

# Change /etc/fstab.
echo >/mnt/etc/fstab "\
# Poor man's File System Table.

root=unknown
usr=unknown"

# How to install?
echo >/mnt/etc/issue "\

Login as root and run 'setup' to install Minix."

umount $rootdev || exit
test "$std" && umount $root 2>/dev/null
installboot -d $rootdev $base/usr/mdec/bootblock boot >/dev/null

# Partition the root floppy whether necessary or not.  (Two images can be
# concatenated, or a combined image can be split later.)
partition -mf $rootdev 0 81:960 0:0 81:1440 81:480 >/dev/null || exit

if [ "$std" ]
then
    if [ "$single" ]
    then
	repartition $rootdev >/dev/null
	usrdev=${rootdev}p2
    else
	echo -n "Insert the usr diskette in drive $drive and hit RETURN"
	read ret
	usrdev=$rootdev
    fi
fi

mkfs $v1 -i 96 $usrdev 720 || exit
mount $usrdev /mnt || exit
install -d -o 0 -g 0 -m 755 /mnt || exit
(cd $base/usr && exec tar cfD - $usrlist) | (cd /mnt && exec tar xvfp -) || exit
umount $usrdev || exit

# Put a "boot the other drive" bootblock on the /usr floppy.
installboot -m $usrdev /usr/mdec/masterboot >/dev/null

# We're done for Minix-vmd here, it has its own ideas on how to package /usr.
test "$std" || exit 0

# Guess the size of /usr in compressed form.  Assume compression down to 60%
# of the original size.  Use "disk megabytes" of 1000*1024 for a safe guess.
set -$- `df | grep "^$usr"`
size=`expr \\( $4 \\* 6 / 10 + 999 \\) / 1000`

echo -n "
You now need enough diskettes to hold /usr in compressed form, close to
$size Mb total.  "

size=
while [ -z "$size" ]
do
    if [ "$single" ]; then defsize=1440; else defsize=720; fi

    echo -n "What is the size of the diskettes? [$defsize] "; read size

    case $size in
    '')	size=$defsize
	;;
    360|720|1200|1440)
	;;
    *)	echo "Sorry, I don't believe \"$size\", try again." >&2
	size=
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

echo "
Enter the floppies in drive $drive when asked to.  Mark them with the volume
numbers!
"
sleep 2

if [ `arch` = i86 ]; then bits=13; else bits=16; fi

>/tmp/DONE
cd /usr && tar cvf - . /tmp/DONE \
    | compress -b$bits | vol -w $size /dev/fd$drive &&
echo Done.
