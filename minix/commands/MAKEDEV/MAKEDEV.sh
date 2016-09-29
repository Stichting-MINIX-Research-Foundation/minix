#!/bin/sh
#
# MAKEDEV 3.3 - Make special devices.			Author: Kees J. Bot
# 3.4 - Rewritten to allow mtree line to be printed,	Lionel A. Sambuc
#       also use getopt for argument parsing
umask 077

MTREE=false
STD=false
RAMDISK=false
ECHO=
EXIT=0

# console => lp tty log
# boot    => kmem mem null ram zero
RAMDISK_DEVICES="
	boot
	console
	input
	c0d0 c0d0p0 c0d0p0s0 c0d1 c0d1p0 c0d1p0s0
	c0d2 c0d2p0 c0d2p0s0 c0d3 c0d3p0 c0d3p0s0
	c0d4 c0d4p0 c0d4p0s0 c0d5 c0d5p0 c0d5p0s0
	c0d6 c0d6p0 c0d6p0s0 c0d7 c0d7p0 c0d7p0s0
	c1d0 c1d0p0 c1d0p0s0 c1d1 c1d1p0 c1d1p0s0
	c1d2 c1d2p0 c1d2p0s0 c1d3 c1d3p0 c1d3p0s0
	c1d4 c1d4p0 c1d4p0s0 c1d5 c1d5p0 c1d5p0s0
	c1d6 c1d6p0 c1d6p0s0 c1d7 c1d7p0 c1d7p0s0
	fd0 fd1 fd0p0 fd1p0
	pci
	ttyc1 ttyc2 ttyc3 tty00 tty01 tty02 tty03
"

STD_DEVICES="
	${RAMDISK_DEVICES}
	bmp085b1s77 bmp085b2s77 bmp085b3s77
	bpf
	eepromb1s50 eepromb1s51 eepromb1s52 eepromb1s53
	eepromb1s54 eepromb1s55 eepromb1s56 eepromb1s57
	eepromb2s50 eepromb2s51 eepromb2s52 eepromb2s53
	eepromb2s54 eepromb2s55 eepromb2s56 eepromb2s57
	eepromb3s50 eepromb3s51 eepromb3s52 eepromb3s53
	eepromb3s54 eepromb3s55 eepromb3s56 eepromb3s57
	fb0 fbd filter hello
	i2c-1 i2c-2 i2c-3
	klog ptmx random
	sht21b1s40 sht21b2s40 sht21b3s40
	tsl2550b1s39 tsl2550b2s39 tsl2550b3s39
	ttyp0 ttyp1 ttyp2 ttyp3 ttyp4 ttyp5 ttyp6 ttyp7 ttyp8 ttyp9
	ttypa ttypb ttypc ttypd ttype ttypf
	ttyq0 ttyq1 ttyq2 ttyq3 ttyq4 ttyq5 ttyq6 ttyq7 ttyq8 ttyq9
	ttyqa ttyqb ttyqc ttyqd ttyqe ttyqf
	vnd0 vnd0p0 vnd0p0s0 vnd1 vnd1p0 vnd1p0s0
	vnd2 vnd3 vnd4 vnd5 vnd6 vnd7
"

#makedev ${dev} $type ${major} ${minor} ${uname} ${gname} ${permissions} [link_target]
#When called for a link, major and minor are ignored
makedev()
{
	# Check that all the arguments are there, we trust the caller to put
	# values which make sens.
	[ $# -eq 7 ] || [ $# -eq 8 ] || return 1;

	local _dev=$1
	local __type=$2
	local _major=$3
	local _minor=$4
	local _uname=$5
	local _gname=$6
	local _mode=$7

	case ${__type} in
	b)_type=block;;
	c) _type=char;;
	l) _type=link; _target=$8;;
	*) return 2;;
	esac

	if [ ${MTREE} = "yes" ]
	then
		if [ ${_type} = "link" ]
		then
			echo ./dev/${_dev} type=${_type} \
			    uname=${_uname} gname=${_gname} mode=${_mode} \
			    link=${_target}
		else
			echo ./dev/${_dev} type=${_type} \
			    uname=${_uname} gname=${_gname} mode=${_mode} \
			    device=native,${_major},${_minor}
		fi
	else
		if [ ${_type} = "link" ]
		then
			${ECHO} ln -f ${_target} ${_dev}
		else
			${ECHO} mknod ${_dev} ${__type} ${_major} ${_minor}
			${ECHO} chmod ${_mode} ${_dev}
			${ECHO} chown ${_uname}:${_gname} ${_dev}
		fi
	fi
}

# no_return usage()
usage()
{
	cat >&2 <<EOF
Usage:	$0 [-n|-m] [-s|-r] key ...
	-n: print the commands instead of executing them
	-m: print mtree(8) line
	-s: standard set of devices
	-r: reduced for ramdisk set of devices

Where key is one of the following:
  ram mem kmem null boot zero	  # One of these makes all these memory devices
  fb0			  # Make /dev/fb0
  i2c-1 i2c-2 i2c-3       # Make /dev/i2c-[1-3]
  tsl2550b{1,3}s39	  # TSL2550 Ambient Light Sensors
  sht21b{1,3}s40	  # SHT21 Relative Humidity and Temperature Sensors
  bmp085b{1,3}s77	  # BMP085 Pressure and Temperature Sensors
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
  audio mixer		  # Make audio devices
  bpf                     # Make /dev/bpf
  klog                    # Make /dev/klog
  ptmx                    # Make /dev/ptmx
  random                  # Make /dev/random, /dev/urandom
  filter                  # Make /dev/filter
  fbd                     # Make /dev/fbd
  hello                   # Make /dev/hello
  video                   # Make /dev/video
  pci                     # Make /dev/pci
  vnd0 vnd0p0 vnd0p0s0 .. # Make vnode disks /dev/vnd[0-7] and (sub)partitions
  input                   # Make /dev/kbdmux, /dev/kbd[0-3], idem /dev/mouse~
EOF
	exit 1
}

# Parse options
while getopts "nmrs" arg
do
	case "$arg" in
	n)
		ECHO=echo
		;;
	m)
		MTREE=yes
		;;
	r)
		RAMDISK=yes
		;;
	s)
		STD=yes
		;;
	h|\?)
		usage
		;;
	esac
done

if [ ${STD} = "yes" ]
then
	DEVICES=${STD_DEVICES}
elif [ ${RAMDISK} = "yes" ]
then
	DEVICES=${RAMDISK_DEVICES}
else
	while [ $OPTIND -gt 1 ]
	do
		shift
		OPTIND=$((${OPTIND} - 1))
	done
	while [ ! -z "$1" ]
	do
		DEVICES="${DEVICES} $1"
		shift
	done
fi

for dev in ${DEVICES}
do
	# Reset the defaults
	uname=root
	gname=wheel
	permissions=600

	case ${dev} in
	c0*) major=3 ;;
	c1*) major=8 ;;
	c2*) major=10 ;;
	c3*) major=12 ;;
	vnd*) # Whole vnode disk devices.
		disk=`expr ${dev} : '...\\(.\\)'`
		major=`expr ${disk} + 56`
		;;
	esac

	# The following is lexicographicaly ordered
	case ${dev} in
	audio|mixer)
		# Audio devices.
		makedev audio c 13 0 ${uname} ${gname} 666
		makedev mixer c 13 1 ${uname} ${gname} 666
		;;
	bmp085b[1-3]s77)
		# Weather Cape: temperature & pressure sensor
		bus=`expr ${dev} : 'bmp085b\\(.*\\)s77'` #bus number
		major=`expr ${bus} + 52`

		makedev bmp085b${bus}s77 c ${major} 0 ${uname} ${gname} 444
		;;
	bpf)
		# Berkeley Packet Filter device, for the LWIP service
		# This is a cloning device, but some programs (e.g., dhclient)
		# assume individual devices are numbered, so also create bpf0.
		makedev ${dev} c 7 0 ${uname} ${gname} 600
		makedev ${dev}0 c 7 0 ${uname} ${gname} 600
		;;
	c[0-3]d[0-7])
		# Whole disk devices.
		disk=`expr ${dev} : '...\\(.\\)'`
		minor=`expr ${disk} '*' 5`

		makedev ${dev} b ${major} ${minor} ${uname} ${gname} ${permissions}
		;;
	c[0-3]d[0-7]p[0-3])
		# Disk primary partitions.
		prefix=`expr ${dev} : '\\(.*\\).'`
		disk=`expr ${dev} : '...\\(.\\)'`

		for p in 0 1 2 3
		do
			minor=`expr ${disk} '*' 5 + 1 + ${p}`
			makedev ${prefix}${p} b ${major} ${minor} \
				${uname} ${gname} ${permissions}
		done
		;;
	c[0-3]d[0-7]p[0-3]s[0-3])
		# Disk subpartition.
		prefix=`expr ${dev} : '\\(.*\\)...'`
		disk=`expr ${dev} : '...\\(.\\)'`

		for p in 0 1 2 3
		do
			for s in 0 1 2 3
			do
				minor=`expr 128 + ${disk} '*' 16 + ${p} '*' 4 + ${s}`
				makedev ${prefix}${p}s${s} b ${major} ${minor} \
					${uname} ${gname} ${permissions}
			done
		done
		;;
	c[0-3]t[0-7]|c[0-3]t[0-7]n)
		# Tape devices.
		prefix=`expr ${dev} : '\\(....\\)'`
		tape=`expr ${dev} : '...\\(.\\)'`
		minor=`expr 64 + ${tape} '*' 2`

		makedev ${prefix}n c ${major} ${minor} ${uname} ${gname} 660
		makedev ${prefix} c ${major} `expr ${minor} + 1` ${uname} ${gname} 660
		;;
	console|lp|tty|log|video)
		# Console, line printer, anonymous tty, diagnostics device, video.
		makedev console c 4 0 ${uname} tty ${permissions}
		makedev tty c 5 0 ${uname} ${gname} 666
		makedev lp c 6 0 daemon daemon 200
		makedev log c 4 15 ${uname} ${gname} 222
		makedev video c 4 125 ${uname} ${gname} ${permissions}
		;;
	eepromb[1-3]s5[0-7])
		# cat24c256 driver
		bus=`expr ${dev} : 'eepromb\\(.*\\)s'`
		# configurable part of slave addr
		slave_low=`expr ${dev} : 'eepromb.s5\\(.*\\)'`
		major=`expr ${bus} '*' 8 + ${slave_low} + 17`

		makedev eepromb${bus}s5${slave_low} b ${major} 0 ${uname} ${gname} ${permissions}
		;;
	fb0)
		# Framebuffer driver
		makedev ${dev} c 19 0 ${uname} ${gname} 644
		;;
	fbd)
		# Faulty block device driver
		makedev ${dev} b 14 0 ${uname} ${gname} ${permissions}
		;;
	fd[0-3])
		# Floppy disk drive N has minor N.
		minor=`expr ${dev} : '..\\(.\\)'`

		makedev ${dev} b 2 ${minor} ${uname} ${gname} 666
		;;
	fd[0-3]p[0-3])
		# Floppy disk partitions.
		prefix=`expr ${dev} : '\\(.*\\).'`
		drive=`expr ${dev} : '..\\(.\\)'`

		for p in 0 1 2 3
		do
			minor=`expr 112 + ${drive} + ${p} '*' 4`
			makedev ${prefix}${p} b 2 ${minor} \
				${uname} ${gname} 666
		done
		;;
	filter)
		# Filter driver
		makedev ${dev} b 11 0 ${uname} ${gname} ${permissions}
		;;
	hello)
		# Hello driver
		makedev ${dev} c 17 0 ${uname} ${gname} 644
		;;
	i2c-[1-3])
		# i2c driver
		bus=`expr ${dev} : '....\\(.*\\)'` # bus number
		# least significant digit of major
		major_low=`expr ${dev} : '....\\(.*\\)'`
		major_low=`expr ${major_low} - 1`

		makedev "i2c-${bus}" c 2${major_low} 0 ${uname} ${gname} ${permissions}
		;;
	input)
		# Input server
		makedev kbdmux c 64 0 ${uname} ${gname} ${permissions}
		makedev mousemux c 64 64 ${uname} ${gname} ${permissions}

		for n in 0 1 2 3
		do
			minor_keyboard=`expr ${n} + 1`
			minor_mouse=`expr ${n} + 65`

			makedev kbd${n} c 64 ${minor_keyboard} ${uname} ${gname} ${permissions}
			makedev mouse${n} c 64 ${minor_mouse} ${uname} ${gname} ${permissions}
		done
		;;
	klog)
		# Logging device.
		makedev ${dev} c 15 0 ${uname} ${gname} ${permissions}
		;;
	pc[0-3]|at[0-3]|qd[0-3]|ps[0-3]|pat[0-3]|qh[0-3]|PS[0-3])
		# Obsolete density locked floppy disk drive n.
		drive=`expr ${dev} : '.*\\(.\\)'`
		minor=${drive}

		for prefix in pc at qd ps pat qh PS
		do
			makedev ${prefix}${drive} b 2 ${minor} \
				${uname} ${gname} 666
			minor=`expr ${minor} + 4`
		done
		;;
	pci)
		# PCI server, manages PCI buses
		makedev pci c 134 0 ${uname} ${gname} ${permissions}
		;;
	ptmx)
		# Unix98 pseudoterminal master
		makedev ptmx c 9 0 ${uname} tty 666
		;;
	ram|mem|kmem|null|boot|zero|imgrd)
		# Memory devices.
		makedev ram   b 1 0 ${uname} kmem ${permissions}
		makedev mem   c 1 1 ${uname} kmem 640
		makedev kmem  c 1 2 ${uname} kmem 640
		makedev null  c 1 3 ${uname} kmem 666
		makedev boot  b 1 4 ${uname} kmem ${permissions}
		makedev zero  c 1 5 ${uname} kmem 644
		makedev imgrd b 1 6 ${uname} kmem ${permissions}

		for n in 0 1 2 3 4 5
		do
			minor=`expr ${n} + 7`

			makedev ram${n} b 1 ${minor} ${uname} kmem ${permissions}
		done
		;;
	random|urandom)
		# Random data generator.
		makedev random c 16 0 ${uname} ${gname} 644
		makedev urandom c 16 0 ${uname} ${gname} 644
		;;
	sht21b[1-3]s40)
		# Weather Cape: relative humidity & temperature sensor
		bus=`expr ${dev} : 'sht21b\\(.*\\)s40'`
		major=`expr ${bus} + 49`

		makedev sht21b${bus}s40 c ${major} 0 ${uname} ${gname} 444
		;;
	tsl2550b[1-3]s39)
		# Weather Cape: ambient light sensor
		bus=`expr ${dev} : 'tsl2550b\\(.*\\)s39'`
		major=`expr ${bus} + 46`

		makedev tsl2550b${bus}s39 c ${major} 0 ${uname} ${gname} 444
		;;
	tty0[0-3])
		# Serial lines.
		line=`expr ${dev} : '.*\\(.\\)'`
		minor=`expr ${line} + 16`

		makedev ${dev} c 4 ${minor} ${uname} tty 666
		;;
	tty[p-s][0-9a-f]|pty[p-s][0-9a-f])
		# Pseudo ttys.
		dev=`expr ${dev} : '...\\(..\\)'`
		group=`expr ${dev} : '\\(.\\)'`
		group=`echo ${group} | tr 'pqrs' '0123'`
		pty=`expr ${dev} : '.\\(.\\)'`
		case ${pty} in
			[a-f])	pty=1`/bin/echo ${pty} | tr 'abcdef' '012345'`
		esac
		minor_tty=`expr ${group} '*' 16 + ${pty} + 128`
		minor_pty=`expr ${group} '*' 16 + ${pty} + 192`

		makedev tty${dev} c 9 ${minor_tty} ${uname} tty 666
		makedev pty${dev} c 9 ${minor_pty} ${uname} tty 666
		;;
	ttyc[1-7])
		# Virtual consoles.
		minor=`expr ${dev} : '....\\(.*\\)'`

		makedev ${dev} c 4 ${minor} ${uname} tty ${permissions}
		;;
	vnd[0-7])
		# Whole vnode disk devices.
		makedev ${dev} b ${major} 0 ${uname} ${gname} ${permissions}
		;;
	vnd[0-7]p[0-3])
		# Vnode disk primary partitions.
		prefix=`expr ${dev} : '\\(.*\\).'`
		disk=`expr ${dev} : '...\\(.\\)'`

		for p in 0 1 2 3
		do
			minor=`expr 1 + ${p}`

			makedev ${prefix}${p} b ${major} ${minor} \
				${uname} ${gname} ${permissions}
		done
		;;
	vnd[0-7]p[0-3]s[0-3])
		# Vnode disk subpartition.
		prefix=`expr ${dev} : '\\(.*\\)...'`
		disk=`expr ${dev} : '...\\(.\\)'`

		for p in 0 1 2 3
		do
			for s in 0 1 2 3
			do
				minor=`expr 128 + ${p} '*' 4 + ${s}`

				makedev ${prefix}${p}s${s} b ${major} ${minor} \
					${uname} ${gname} ${permissions}
			done
		done
		;;
	*)
		echo "$0: don't know about ${dev}" >&2
		EXIT=1
	esac
done

exit $EXIT
