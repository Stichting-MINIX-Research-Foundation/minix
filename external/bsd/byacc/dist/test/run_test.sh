#!/bin/sh
# Id: run_test.sh,v 1.6 2010/06/08 08:53:38 tom Exp
# vi:ts=4 sw=4:

if test $# = 1
then
	PROG_DIR=`pwd`
	TEST_DIR=$1
else
	PROG_DIR=..
	TEST_DIR=.
fi

YACC=$PROG_DIR/yacc

tmpfile=temp$$

echo '** '`date`
for i in ${TEST_DIR}/*.y
do
	case $i in
	test*)
		echo "?? ignored $i"
		;;
	*)
		root=`basename $i .y`
		ROOT="test-$root"
		prefix=${root}_

		OPTS=
		TYPE=".output .tab.c .tab.h"
		case $i in
		${TEST_DIR}/code_*)
			OPTS="$OPTS -r"
			TYPE="$TYPE .code.c"
			prefix=`echo "$prefix" | sed -e 's/^code_//'`
			;;
		${TEST_DIR}/pure_*)
			OPTS="$OPTS -P"
			prefix=`echo "$prefix" | sed -e 's/^pure_//'`
			;;
		esac

		$YACC $OPTS -v -d -p $prefix -b $ROOT $i
		for type in $TYPE
		do
			REF=${TEST_DIR}/${root}${type}
			CMP=${ROOT}${type}
			if test ! -f $CMP ; then
				echo "...not found $CMP"
				continue
			fi
			sed	-e s,$CMP,$REF, \
				-e /YYPATCH/d \
				-e 's,#line \([1-9][0-9]*\) "'$TEST_DIR'/,#line \1 ",' \
				< $CMP >$tmpfile \
				&& mv $tmpfile $CMP
			if test ! -f $REF
			then
				mv $CMP $REF
				echo "...saved $REF"
			elif ( cmp -s $REF $CMP )
			then
				echo "...ok $REF"
				rm -f $CMP
			else
				echo "...diff $REF"
				diff -u $REF $CMP
			fi
		done
		;;
	esac
done
