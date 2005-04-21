#!/bin/sh
#
#	checkhier 2.7 - check the directory hierarchy	Author: Kees J. Bot
#								7 May 1995

case "`id`" in
'uid=0('*)	;;
*)	echo "$0: must be run by root" >&2
	exit 1
esac

# List of all interesting top level files and directories, with mode,
# owner and group.  Only the key files are listed, the rest is owned
# by bin, has mode 755 or 644, and is not critical to the operation of
# the system.
{
	cat <<'EOF'
drwxr-xr-x	root	operator	/
drwxr-xr-x	bin	operator	/bin
drwxr-xr-x	root	operator	/dev
drwxr-xr-x	root	operator	/etc
-rw-r--r--	root	operator	/etc/fstab
-rw-r--r--	root	operator	/etc/group
-rw-r--r--	root	operator	/etc/hostname.file
-rw-r--r--	root	operator	/etc/inet.conf
-rw-r--r--	root	operator	/etc/motd
-rw-r--r--	root	operator	/etc/mtab
-rw-r--r--	root	operator	/etc/passwd
-rw-r--r--	root	operator	/etc/profile
-rw-r--r--	root	operator	/etc/protocols
-rw-r--r--	root	operator	/etc/rc
-rw-r--r--	root	operator	/etc/services
-rw-------	root	operator	/etc/shadow
-rw-r--r--	root	operator	/etc/termcap
-rw-r--r--	root	operator	/etc/ttytab
-rw-r--r--	root	operator	/etc/utmp
dr-xr-xr-x	root	operator	/fd0
dr-xr-xr-x	root	operator	/fd1
dr-xr-xr-x	root	operator	/mnt
dr-xr-xr-x	root	operator	/root
drwxrwxrwx	root	operator	/tmp
drwxr-xr-x	root	operator	/usr
drwxr-xr-x	root	operator	/usr/adm
-rw-r--r--	root	operator	/usr/adm/lastlog
-rw-r--r--	root	operator	/usr/adm/wtmp
drwxr-xr-x	ast	other		/usr/ast
drwxr-xr-x	bin	operator	/usr/bin
drwxr-xr-x	root	operator	/usr/etc
drwxr-xr-x	bin	operator	/usr/include
drwxr-xr-x	bin	operator	/usr/lib
drwxrwxr-x	root	operator	/usr/local
drwxrwxr-x	bin	operator	/usr/local/bin
drwxrwxr-x	bin	operator	/usr/local/include
drwxrwxr-x	bin	operator	/usr/local/lib
drwxrwxr-x	bin	operator	/usr/local/man
drwxrwxr-x	bin	operator	/usr/local/src
drwxr-xr-x	bin	operator	/usr/man
drwxr-xr-x	bin	operator	/usr/mdec
drwx------	root	operator	/usr/preserve
drwxr-xr-x	root	operator	/usr/run
drwxr-xr-x	root	operator	/usr/spool
drwx--x--x	root	operator	/usr/spool/at
drwx--x--x	root	operator	/usr/spool/at/past
drwx------	root	operator	/usr/spool/crontabs
drwxrwxr-x	root	uucp		/usr/spool/locks
drwx------	daemon	daemon		/usr/spool/lpd
drwxr-xr-x	bin	operator	/usr/src
drwxrwxrwx	root	operator	/usr/tmp
-rwsr-xr-x	root	?		/usr/bin/at
-rwsr-xr-x	root	?		/usr/bin/chfn
-rwsr-xr-x	root	?		/usr/bin/chsh
-rwsr-xr-x	root	?		/usr/bin/df
-rwsr-xr-x	root	?		/usr/bin/elvprsv
-rwsr-xr-x	root	?		/usr/bin/elvrec
-rwsr-xr-x	root	?		/usr/bin/format
-rwsr-xr-x	root	?		/usr/bin/hostaddr
-rwsr-xr-x	root	?		/usr/bin/install
-rwsr-xr-x	daemon	?		/usr/bin/lpd
-rwsr-xr-x	root	?		/usr/bin/mail
-rwsr-xr-x	root	?		/usr/bin/mount
-rwsr-xr-x	root	?		/usr/bin/passwd
-rwsr-xr-x	root	?		/usr/bin/ping
-rwxr-sr-x	?	kmem		/usr/bin/ps
-rwsr-xr--	root	?		/usr/bin/shutdown
-rwsr-xr-x	root	?		/usr/bin/su
-rwxr-sr-x	?	uucp		/usr/bin/term
-rwsr-xr-x	root	?		/usr/bin/umount
-rwxr-sr-x	?	tty		/usr/bin/write
EOF

} | {
	# Check if each file has the proper attributes.  Offer a correction
	# if not.
	banner="\
# List of commands to fix the top level hierarchy.  Do not apply these
# commands blindly, but check and repair by hand.
"

	while read mode owner group file
	do
	    ( # "fix" a memory leak in set...

		set -$- `ls -ld $file 2>/dev/null` '' '' '' ''
		curmode=$1 curowner=$3 curgroup=$4
		test $owner = '?' && curowner=$owner
		test $group = '?' && curgroup=$group

		# File types?
		if [ x`expr "$mode" : '\\(.\\)'` != \
					x`expr "$curmode" : '\\(.\\)'` ]
		then
			case $curmode in
			?*)	echo "${banner}rm -r $file"
				banner=
			esac
			curmode= curowner= curgroup=
			case $mode in
			d*)	echo "${banner}mkdir $file"
				;;
			-*)	echo "${banner}> $file"
				;;
			*)	echo "$0: $mode $file: unknown filetype" >&2
				exit 1
			esac
			banner=
		fi

		# Mode?
		if [ x$mode != x$curmode ]
		then
			octmode=
			m=$mode
			for i in u g o
			do
				r=0 w=0 x=0
				case $m in
				?r??*)		r=4
				esac
				case $m in
				??w?*)		w=2
				esac
				case $m in
				???[xst]*)	x=1
				esac
				octmode=$octmode`expr $r + $w + $x`
				m=`expr $m : '...\\(.*\\)'`
			done
			r=0 w=0 x=0
			case $mode in
			???[sS=]??????)	r=4
			esac
			case $mode in
			??????[sS=]???)	w=2
			esac
			case $mode in
			?????????[tT=])	x=1
			esac
			case $r$w$x in
			000)	;;
			*)	octmode=`expr $r + $w + $x`$octmode
			esac

			echo "${banner}chmod $octmode $file"
			banner=
		fi

		# Ownership?
		if [ x$owner != x$curowner -o x$group != x$curgroup ]
		then
			echo "${banner}chown $owner:$group $file"
			banner=
		fi

		# The Minix shell forgets processes, so wait explicitly.
		wait

	    case "$banner" in '') exit 1;; *) exit 0;; esac) || banner=
	done
	case "$banner" in
	'')	exit 1
	esac
	exit 0
}
