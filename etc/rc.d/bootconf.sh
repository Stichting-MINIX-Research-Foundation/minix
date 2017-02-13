#!/bin/sh
#
# $NetBSD: bootconf.sh,v 1.15 2012/12/31 23:21:27 christos Exp $
#

# PROVIDE: bootconf
# REQUIRE: mountcritlocal
# KEYWORD: interactive

$_rc_subr_loaded . /etc/rc.subr

name="bootconf"
start_cmd="bootconf_start"
stop_cmd=":"

bootconf_start()
{
		# Refer to newbtconf(8) for more information
		#

	if [ ! -e /etc/etc.current ]; then
		return 0
	fi
	if [ -h /etc/etc.default ]; then
		def=$(ls -ld /etc/etc.default 2>&1)
		default="${def##*-> *etc.}"
	else
		default=current
	fi
	if [ "$default" = "current" ]; then
		def=$(ls -ld /etc/etc.current 2>&1)
		default="${def##*-> *etc.}"
	fi

	spc=""
	for i in /etc/etc.*; do
		name="${i##/etc/etc.}"
		case $name in
		current|default|\*)
			continue
			;;	
		*)
			if [ "$name" = "$default" ]; then
				echo -n "${spc}[${name}]"
			else
				echo -n "${spc}${name}"
			fi
			spc=" "
			;;
		esac
	done
	echo
	_DUMMY=/etc/passwd
	conf=${_DUMMY}
	while [ ! -d /etc/etc.$conf/. ]; do
		trap "conf=$default; echo; echo Using default of $default" ALRM
		echo -n "Which configuration [$default] ? "
		(sleep 30 && kill -ALRM $RC_PID) >/dev/null 2>&1 &
		read conf
		trap : ALRM
		if [ -z $conf ] ; then
			conf=$default
		fi
		if [ ! -d /etc/etc.$conf/. ]; then
			conf=${_DUMMY}
		fi
	done

	print_rc_metadata "note:Using configuration \"${conf}\""

	case  $conf in
	current|default)
		;;
	*)
		rm -f /etc/etc.current
		ln -s etc.$conf /etc/etc.current
		sync
		;;
	esac

	if [ -f /etc/rc.conf ] ; then
		. /etc/rc.conf
	fi
}

load_rc_config $name
run_rc_command "$1"
