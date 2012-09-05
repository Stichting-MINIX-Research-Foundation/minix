
if [ ! "$MAKE" ]; then	MAKE=make; fi
ITERATIONS=5
PRECMD="$MAKE clean"
COMMAND="$MAKE all"
TAG=time.$(basename $(git --git-dir=/usr/src/.git describe --all --dirty))

set -e

while getopts "n:d:p:c:r:s" c
do
        case "$c" in
		s)	PROFILE=1 ;;
		n)	ITERATIONS=$OPTARG ;;
		p)	PRECMD="$OPTARG" ;;
		c)	COMMAND="$OPTARG" ;;
		t)	TAG=$OPTARG	;;
		r)	echo "Reading settings from $OPTARG"; cat $OPTARG; . $OPTARG ; echo "Reading done.";;
		*)	exit 1	;;
	esac
done

CONFIGPREFIX=".worldstone"
CONFIGVARS="ITERATIONS PRECMD COMMAND MAKE"
TMPF=.worldstone.tmpconfig.$$
rm -f $TMPF
for d in $CONFIGVARS
do	eval "echo $d=\\\"\$$d\\\"" >>$TMPF
done
CONFIGTAG=`crc <$TMPF | awk '{ print $1 }'`
CONFIGFILE=$CONFIGPREFIX.$CONFIGTAG
mv -f $TMPF $CONFIGFILE

LOGFILE=$TAG.worldstone.log

while [ -f $LOGFILE ]
do	echo "$0: WARNING: $LOGFILE already exists, appending."
	LOGFILE=$LOGFILE.next
done

echo "Logging to $LOGFILE."

echo "First run."
sh -c "$PRECMD"
sh -c "$COMMAND"

if [ "$PROFILE" ]; then profile stop || true; fi

for n in `seq 1 $ITERATIONS`
do	echo -n "$n"
	sh -c "$PRECMD >/dev/null 2>&1"
	echo -n "."
	sync
	PROF=$LOGFILE.p.$n
	if [ "$PROFILE" ]; then profile start --rtc -o $PROF -f 3; fi
	time -C sh -c "$COMMAND >/dev/null 2>&1; sync" 2>>$LOGFILE
	if [ "$PROFILE" ]; then profile stop; sprofalyze -d $PROF >$PROF.d; fi
	echo -n " "
done
echo "Done."
echo "Time measurements logfile is $LOGFILE."
echo "Config file is $CONFIGFILE."
