#!/bin/sh
set -e

ROOT=`printroot -r`
DEFAULTCFG=/etc/boot.cfg.default
LOCALCFG=/etc/boot.cfg.local
TMP=/boot.cfg.temp
DIRSBASE=/boot/minix
INHERIT="ahci acpi no_apic nobeep"

filter_entries()
{
	# This routine performs three tasks:
	# - substitute variables in the configuration lines;
	# - remove multiboot entries that refer to nonexistent kernels;
	# - adjust the default option for line removal and different files.
	# The last part is handled by the awk part of the routine.

	while read line
	do
		# Substitute variables like $rootdevname and $args
		line=$(eval echo \"$line\")

		if ! echo "$line" | grep -sq '^menu=.*multiboot'
		then
			echo "$line"
			continue
		fi

		# Check if the referenced kernel is present
		kernel=`echo "$line" | sed -n 's/.*multiboot[[:space:]]*\(\/[^[:space:]]*\).*/\1/p'`
		if [ ! -r "$kernel" ]
		then
			echo "Warning: config contains entry for \"$kernel\" which is missing! Entry skipped." 1>&2
			echo "menu=SKIP"
		else
			echo "$line"
		fi
	done | awk '
		BEGIN {
			count=1
			base=0
			default=0
		}
		/^menu=SKIP$/ {
			# A menu entry discarded by the kernel check above
			skip[count++]=1
			next
		}
		/^menu=/ {
			# A valid menu entry
			skip[count++]=0
			print
			next
		}
		/^BASE=/ {
			# The menu count base as passed in by count_entries()
			sub(/^.*=/,"",$1)
			base=$1+0
			next
		}
		/^default=/ {
			# The default option
			# Correct for the menu count base and store for later
			sub(/^.*=/,"",$1)
			default=$1+base
			next
		}
		{
			# Any other line
			print
		}
		END {
			# If a default was given, correct for removed lines
			# (do not bother to warn if the default itself is gone)
			if (default) {
				for (i = default; i > 0; i--)
					if (skip[i]) default--;
				print "default=" default
			}
		}
	'
}

count_entries()
{
	echo -n "BASE="; grep -cs '^menu=' "$1"
}

if [ ! -b "$ROOT" ]
then
	echo root device $ROOT not found
	exit 1
fi

rootdevname=`echo $ROOT | sed 's/\/dev\///'`

# Construct a list of inherited arguments for boot options to use. Note that
# rootdevname must not be passed on this way, as it is changed during setup.
args=""
for k in $INHERIT; do
	if sysenv | grep -sq "^$k="; then
		kv=$(sysenv | grep "^$k=")
		args="$args $kv"
	fi
done

if [ -r $DEFAULTCFG ]
then
	filter_entries < $DEFAULTCFG >> $TMP
fi

if [ -d /boot/minix_latest -o -h /boot/minix_latest ]
then
	latest=`basename \`stat -f "%Y" /boot/minix_latest\``
fi

[ -d $DIRSBASE ] && for i in `ls $DIRSBASE/`
do
	build_name="`basename $i`"
	if [ "$build_name" != "$latest" ]
	then
		echo "menu=Start MINIX 3 ($build_name):load_mods $DIRSBASE/$i/mod*;multiboot $DIRSBASE/$i/kernel rootdevname=$rootdevname $args" >> $TMP
	fi
done

if [ -r $LOCALCFG ]
then
	# If the local config supplies a "default" option, we assume that this
	# refers to one of the options in the local config itself. Therefore,
	# we increase this default by the number of options already present in
	# the output so far. To this end, count_entries() inserts a special
	# token that is recognized and filtered out by filter_entries().
	(count_entries $TMP; cat $LOCALCFG) | filter_entries >> $TMP
fi

mv $TMP /boot.cfg

sync
