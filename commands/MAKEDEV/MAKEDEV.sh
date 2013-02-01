#!/bin/sh
#
# MAKEDEV 3.3 - Make special devices.			Author: Kees J. Bot

case $1 in
-n)	e=echo; shift ;;	# Just echo when -n is given.
*)	e=
esac

case $#:$1 in
1:std)		# Standard devices.
    set -$- mem fd0 fd1 fd0p0 fd1p0 \
	c0d0 c0d0p0 c0d0p0s0 c0d1 c0d1p0 c0d1p0s0 \
	c0d2 c0d2p0 c0d2p0s0 c0d3 c0d3p0 c0d3p0s0 \
	c0d4 c0d4p0 c0d4p0s0 c0d5 c0d5p0 c0d5p0s0 \
	c0d6 c0d6p0 c0d6p0s0 c0d7 c0d7p0 c0d7p0s0 \
	c1d0 c1d0p0 c1d0p0s0 c1d1 c1d1p0 c1d1p0s0 \
	c1d2 c1d2p0 c1d2p0s0 c1d3 c1d3p0 c1d3p0s0 \
	c1d4 c1d4p0 c1d4p0s0 c1d5 c1d5p0 c1d5p0s0 \
	c1d6 c1d6p0 c1d6p0s0 c1d7 c1d7p0 c1d7p0s0 \
	tty ttyc1 ttyc2 ttyc3 tty00 tty01 tty02 tty03 \
	ttyp0 ttyp1 ttyp2 ttyp3 ttyp4 ttyp5 ttyp6 ttyp7 ttyp8 ttyp9 \
	ttypa ttypb ttypc ttypd ttype ttypf \
	ttyq0 ttyq1 ttyq2 ttyq3 ttyq4 ttyq5 ttyq6 ttyq7 ttyq8 ttyq9 \
	ttyqa ttyqb ttyqc ttyqd ttyqe ttyqf \
	eth klog random uds filter fbd hello fb0
    ;;
0:|1:-\?)
    cat >&2 <<EOF
Usage:	$0 [-n] key ...
Where key is one of the following:
  ram mem kmem null boot zero	  # One of these makes all these memory devices
  fb0			  # Make /dev/fb0
  fd0 fd1 ...		  # Floppy devices for drive 0, 1, ...
  fd0p0 fd1p0 ...	  # Make floppy partitions fd0p[0-3], fd1p[0-3], ...
  c0d0 c0d1 ...		  # Make disks c0d0, c0d1, ...
  c0d0p0 c0d1p0 ...	  # Make partitions c0d0p[0-3], c0d1p[0-3], ...
  c0d0p0s0 c0d1p0s0 ...	  # Subparts c0d0p[0-3]s[0-3], c0d1p[0-3]s[0-3], ...
  c1d0(p0)(s0)		  # Likewise for controller 1
  c0t0 c0t1 c1t0 ...	  # Make tape devices c0t0, c0t0n, c0t1, ...
  console lp tty log	  # One of these makes all four
  ttyc1 ... ttyc7         # Virtual consoles
  tty00 ... tty03         # Make serial lines
  ttyp0 ... ttyq0 ...     # Make tty, pty pairs
  eth ip tcp udp	  # One of these makes some TCP/IP devices
  audio mixer		  # Make audio devices
  klog                    # Make /dev/klog
  random                  # Make /dev/random, /dev/urandom
  uds                     # Make /dev/uds
  kbd                     # Make /dev/kbd
  kbdaux                  # Make /dev/kbdaux
  filter                  # Make /dev/filter
  fbd                     # Make /dev/fbd
  hello                   # Make /dev/hello
  video                   # Make /dev/video
  std			  # All standard devices
EOF
    exit 1
esac

umask 077
ex=0

for dev
do
    case $dev in	# One of the controllers?  Precompute major device nr.
    c0*) maj=3  ;;
    c1*) maj=8  ;;
    c2*) maj=10 ;;
    c3*) maj=12 ;;
    esac

    case $dev in
    ram|mem|kmem|null|boot|zero|imgrd)
	# Memory devices.
	#
	$e mknod ram b 1 0;	$e chmod 600 ram
	$e mknod mem c 1 1;	$e chmod 640 mem
	$e mknod kmem c 1 2;	$e chmod 640 kmem
	$e mknod null c 1 3;	$e chmod 666 null
	$e mknod boot b 1 4;	$e chmod 600 ram
	$e mknod zero c 1 5;	$e chmod 644 zero
	$e mknod imgrd b 1 6;	$e chmod 644 zero
	for n in 0 1 2 3 4 5
	do	$e mknod ram$n b 1 $((7+$n));	$e chmod 600 ram$n
	done
	$e chgrp kmem ram* mem kmem null boot zero imgrd
	;;
    fd[0-3])
	# Floppy disk drive n.
	#
	d=`expr $dev : '.*\\(.\\)'`	# Drive number.
	$e mknod $dev  b 2 $d
	$e chmod 666 $dev
	;;
    pc[0-3]|at[0-3]|qd[0-3]|ps[0-3]|pat[0-3]|qh[0-3]|PS[0-3])
	# Obsolete density locked floppy disk drive n.
	#
	d=`expr $dev : '.*\\(.\\)'`	# Drive number.
	m=$d				# Minor device number.

	$e mknod pc$d  b 2 $m;	m=`expr $m + 4`
	$e mknod at$d  b 2 $m;	m=`expr $m + 4`
	$e mknod qd$d  b 2 $m;	m=`expr $m + 4`
	$e mknod ps$d  b 2 $m;	m=`expr $m + 4`
	$e mknod pat$d b 2 $m;	m=`expr $m + 4`
	$e mknod qh$d  b 2 $m;	m=`expr $m + 4`
	$e mknod PS$d  b 2 $m;	m=`expr $m + 4`

	$e chmod 666 pc$d at$d qd$d ps$d pat$d qh$d PS$d
	;;
    fd[0-3]p[0-3])
	# Floppy disk partitions.
	#
	n=`expr $dev : '\\(.*\\)..'`	# Name prefix.
	d=`expr $dev : '..\\(.\\)'`	# Drive number.
	m=`expr 112 + $d`		# Minor of partition 0.
	alldev=

	for p in 0 1 2 3
	do
	    m=`expr 112 + $d + $p '*' 4` # Minor of partition $p.
	    $e mknod ${n}p${p} b 2 $m	# Make e.g. fd0p0 - fd0p3
	    alldev="$alldev ${n}p${p}"
	done
	$e chmod 666 $alldev
	;;
    c[0-3]d[0-7])
	# Whole disk devices.
	d=`expr $dev : '...\\(.\\)'`	# Disk number.
	m=`expr $d '*' 5`		# Minor device number.
	$e mknod $dev b $maj $m
	$e chmod 600 $dev
	;;
    c[0-3]d[0-7]p[0-3])
	# Disk primary partitions.
	n=`expr $dev : '\\(.*\\).'`	# Name prefix.
	d=`expr $dev : '...\\(.\\)'`	# Disk number.
	alldev=

	for p in 0 1 2 3
	do
	    m=`expr $d '*' 5 + 1 + $p`	# Minor device number.
	    $e mknod $n$p b $maj $m
	    alldev="$alldev $n$p"
	done
	echo $alldev | xargs $e chmod 600
	;;
    c[0-3]d[0-7]p[0-3]s[0-3])
	# Disk subpartition.
	n=`expr $dev : '\\(.*\\)...'`	# Name prefix.
	d=`expr $dev : '...\\(.\\)'`	# Disk number.
	alldev=

	for p in 0 1 2 3
	do
	    for s in 0 1 2 3
	    do
		m=`expr 128 + $d '*' 16 + $p '*' 4 + $s`  # Minor device nr.
		$e mknod ${n}${p}s${s} b $maj $m
		alldev="$alldev ${n}${p}s${s}"
	    done
	done
	echo $alldev | xargs $e chmod 600
	;;
    c[0-3]t[0-7]|c[0-3]t[0-7]n)
	# Tape devices.
	n=`expr $dev : '\\(....\\)'`	# Name prefix.
	t=`expr $dev : '...\\(.\\)'`	# Tape number.
	m=`expr 64 + $t '*' 2`		# Minor device number.
	$e mknod ${n}n c $maj $m
	$e mknod ${n} c $maj `expr $m + 1`
	$e chmod 660 ${n}n ${n}
	;;
    console|lp|tty|log|kbd|kbdaux|video)
	# Console, line printer, anonymous tty, diagnostics device,
	# raw keyboard, ps/2 mouse, video.
	$e mknod console c 4 0
	$e chmod 600 console
	$e chgrp tty console
	$e mknod tty c 5 0
	$e chmod 666 tty
	$e mknod lp c 6 0
	$e chown daemon lp
	$e chgrp daemon lp
	$e chmod 200 lp
	$e mknod log c 4 15
	$e chmod 222 log
	$e mknod kbd c 4 127
	$e mknod kbdaux c 4 126
	$e chmod 660 kbd kbdaux
	$e chgrp operator kbd kbdaux
	$e mknod video c 4 125
	$e chmod 600 video
	$e chgrp operator video
	;;
    ttyc[1-7])
	# Virtual consoles.
	#
	m=`expr $dev : '....\\(.*\\)'`	# Minor device number.
	$e mknod $dev c 4 $m
	$e chgrp tty $dev
	$e chmod 600 $dev
	;;
    tty0[0-3])
	# Serial lines.
	#
	n=`expr $dev : '.*\\(.\\)'`
	$e mknod $dev c 4 `expr $n + 16`
	$e chmod 666 $dev
	$e chgrp tty $dev
	;;
    tty[p-s][0-9a-f]|pty[p-s][0-9a-f])
	# Pseudo ttys.
	#
	dev=`expr $dev : '...\\(..\\)'`
	g=`expr $dev : '\\(.\\)'`	# Which group.
	g=`echo $g | tr 'pqrs' '0123'`
	n=`expr $dev : '.\\(.\\)'`	# Which pty in the group.
	case $n in
	[a-f])	n=1`/bin/echo $n | tr 'abcdef' '012345'`
	esac

	$e mknod tty$dev c 4 `expr $g '*' 16 + $n + 128`
	$e mknod pty$dev c 4 `expr $g '*' 16 + $n + 192`
	$e chgrp tty tty$dev pty$dev
	$e chmod 666 tty$dev pty$dev
	;;
    eth|ip|tcp|udp|eth0|ip0|tcp0|udp0)
	# TCP/IP devices.
	#
	$e mknod eth0 c 7 0		# Network 0 (Ethernet)
	$e mknod ip0 c 7 1
	$e mknod tcp0 c 7 2
	$e mknod udp0 c 7 3
	$e chmod 600 eth0 ip0
	$e chmod 666 tcp0 udp0
	$e ln -f eth0 eth		# Default interface
	$e ln -f ip0 ip
	$e ln -f tcp0 tcp
	$e ln -f udp0 udp
	;;
    audio|mixer)
	# Audio devices.
	#
   $e mknod audio c 13 0
   $e mknod mixer c 13 1
   $e chmod 666 audio mixer
	;;
    random|urandom)
	# random data generator.
	$e mknod random c 16 0;	$e chmod 644 random
	$e mknod urandom c 16 0; $e chmod 644 urandom
	$e chgrp operator random urandom
	;;
    uds)
	# unix domain sockets device
	$e mknod uds c 18 0;
	$e chgrp operator uds
	$e chmod 666 uds
	;;
    klog)
    	# logging device.
    	$e mknod klog c 15 0
	$e chmod 600 klog
	;;
    filter)
	# filter driver
	$e mknod filter b 11 0
	$e chmod 600 filter
	;;
    fbd)
	# faulty block device driver
	$e mknod fbd b 14 0
	$e chmod 600 fbd
	;;
    hello)
	# hello driver
	$e mknod hello c 17 0
	$e chmod 644 hello
	;;
    fb0)
	# framebuffer driver
	$e mknod fb0 c 19 0
	$e chmod 644 fb0
	;;
    *)
	echo "$0: don't know about $dev" >&2
	ex=1
    esac
done

exit $ex
