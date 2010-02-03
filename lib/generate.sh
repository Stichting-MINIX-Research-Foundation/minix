#!/bin/sh
# called with parameters: 1:dir 2:ackbase 3:gnubase

set -e

. /etc/make.conf

exec  >Makefile
exec 3>Makedepend-ack
exec 4>Makedepend-gnu
touch  .depend-ack
touch  .depend-gnu

echo "#Generated from $1/Makefile.in"

ACKBASE=$2
GNUBASE=$3
OBJDIR=$1

RECURSIVE_TARGETS="clean depend depend-ack depend-gnu"

if [ -z $ACKBASE ]; then echo ACKBASE is not set!; exit 1; fi
if [ -z $GNUBASE ]; then echo GNUBASE is not set!; exit 1; fi

. Makefile.in

#to enable library debugging, enable the next line
#CFLAGS=$CFLAGS" -g"

echo "all: all-ack"
echo
echo "all-ack:"
echo "all-gnu:"
echo
echo "makefiles: Makefile"
echo "Makedepend-ack Makedepend-gnu: "
echo "	sh $0 $OBJDIR $ACKBASE $GNUBASE"
echo
echo "Makefile: Makefile.in Makedepend-ack Makedepend-gnu"
echo "	sh $0 $OBJDIR $ACKBASE $GNUBASE"
echo "	@echo"
echo "	@echo *Attention*"
echo "	@echo Makefile is regenerated... rerun command to see changes"
echo "	@echo *Attention*"
echo "	@echo"
echo
if [ ! -z "$SUBDIRS" ]; then
	echo "all-ack: makefiles"
	for dir in $SUBDIRS
	{
		if [ $TYPE = "both" -o $TYPE = "ack" ]; then
			echo "	mkdir -p $ACKBASE/$OBJDIR/$dir"
		fi
		echo "	cd $dir && \$(MAKE) \$@"
	}
	echo
	echo "all-gnu: makefiles"
	for dir in $SUBDIRS
	{
		if [ $TYPE = "both" -o $TYPE = "gnu" ]; then
			echo "	mkdir -p $GNUBASE/$OBJDIR/$dir"
		fi
		
		echo "	cd $dir && \$(MAKE) \$@"
	}
	echo
	echo "$RECURSIVE_TARGETS:: makefiles"
	for dir in $SUBDIRS
	{
		#if [ $TYPE = "both" -o $TYPE = "ack" ]; then
			#echo "	mkdir -p $ACKBASE/$OBJDIR/$dir"
		#fi
		#if [ $TYPE = "both" -o $TYPE = "gnu" ]; then
			#echo "	mkdir -p $GNUBASE/$OBJDIR/$dir"
		#fi
		
		echo "	cd $dir && \$(MAKE) \$@"
	}
	echo
	for dir in $SUBDIRS
	{
		echo "makefiles: $dir/Makefile"
	}
	echo
	for dir in $SUBDIRS
	{
		echo "$dir/Makefile: $dir/Makefile.in"
		echo "	cd $dir && sh ../$0 $OBJDIR/$dir ../$ACKBASE ../$GNUBASE && \$(MAKE) makefiles"
	}
else

echo "depend: depend-ack"

echo "depend-ack:" >&3
echo "	rm .depend-ack" >&3
echo "	touch .depend-ack" >&3

echo "depend-gnu:" >&4
echo "	rm .depend-gnu" >&4
echo "	touch .depend-gnu" >&4

ackCommands()
{
	dstfile=$1
	srcfile=$2
	dstdir=`dirname $dstfile`
	
	case $srcfile in
	*.s | *.c | *.e )
		echo "	cc $CFLAGS -c -o $dstfile $srcfile"
		
		echo "	mkdep 'cc $CFLAGS -E' $srcfile | sed -e 's:^\(.\):$dstdir/\1:' >> .depend-ack" >&3
		;;
	*.mod )
		echo "	m2 $M2FLAGS -c -o $dstfile $srcfile"
		
		echo "	mkdep 'm2 $M2FLAGS -E' $srcfile | sed -e 's:^\(.\):$dstdir/\1:' >> .depend-ack" >&3
		;;
	*.fc )
		echo "	sh ./FP.compile $dstfile $srcfile"
		
		echo "	mkdep 'cc -E' $srcfile | sed -e 's:^\(.\):$dstdir/\1:' >> .depend-ack" >&3
		;;
	esac
}

gnuCommands()
{
	dstfile=$1
	srcfile=$2
	dstdir=`dirname $dstfile`
	sedcmd="sed -e '/<built-in>/d' -e '/<command line>/d' -e 's:^\(.\):$dstdir/\1:'"
	
	case $srcfile in
	*.s )
		echo "	gcc $CFLAGS -E -x assembler-with-cpp -I. $srcfile | asmconv -mi386 ack gnu > $GNUBASE/$OBJDIR/$srcfile.gnu"
		echo "	gas -o $dstfile $GNUBASE/$OBJDIR/$srcfile.gnu"
		
		echo "	mkdep 'gcc $CFLAGS -E -x assembler-with-cpp -I.' $srcfile | $sedcmd >> .depend-gnu" >&4
		;;
	*.gs )
		echo "	gas -o $dstfile $srcfile"
		
		echo "	mkdep 'gcc $CFLAGS -E -x assembler-with-cpp -I.' $srcfile | $sedcmd >> .depend-gnu" >&4
		;;
	*.c )
		echo "	gcc $CFLAGS -c -o $dstfile $srcfile"
		
		echo "	mkdep 'gcc $CFLAGS -E' $srcfile | $sedcmd >> .depend-gnu" >&4
		;;
	#*.mod )
	#	echo "	\$(M2C) -o $dstfile $srcfile"
	#	;;
	#*.fc )
	#	echo "	sh ./FP.COMPILE $srcfile"
	#	;;
	esac
	echo
}

#libraries
for lib in $LIBRARIES
{
	if [ $TYPE = "both" -o $TYPE = "ack" ]; then
		echo "all-ack: $ACKBASE/$lib.a"
		eval "FILES=\$${lib}_FILES" 
		echo
		for f in $FILES
		{
			o=`echo $f | sed -e 's/\\..*\$/\.o/'`
			echo "$ACKBASE/$lib.a: $ACKBASE/$lib.a($o)"
		}
		echo
		echo "$ACKBASE/$lib.a:"
		echo "	ar cr $ACKBASE/$lib.a $ACKBASE/$OBJDIR/*.o"
		echo "	rm $ACKBASE/$OBJDIR/*.o"
		echo
		for f in $FILES
		{
			o=`echo $f | sed -e 's/\\..*\$/\.o/'`
			echo "$ACKBASE/$lib.a($o): $f"
			
			ackCommands $ACKBASE/$OBJDIR/$o $f
		}
		echo
	fi
	
	if [ $TYPE = "both" -o $TYPE = "gnu" ]; then
		echo "all-gnu: $GNUBASE/$lib.a"
		eval "FILES=\$${lib}_FILES" 
		echo
		for f in $FILES
		{
			o=`echo $f | sed -e 's/\\..*\$/\.o/'`
			echo "$GNUBASE/$lib.a: $GNUBASE/$OBJDIR/$o"
		}
		echo
		echo "$GNUBASE/$lib.a:"
		echo "	gar cr $GNUBASE/$lib.a \$?"
		echo
		for f in $FILES
		{
			o=`echo $f | sed -e 's/\\..*\$/\.o/'`
			
			echo "$GNUBASE/$OBJDIR/$o: $f"
			
			gnuCommands $GNUBASE/$OBJDIR/$o $f
		}
		echo
	fi
}
echo

#start files
for f in $STARTFILES
{
	o=`echo $f | sed -e 's/\\..*\$/\.o/'`
	
	if [ $TYPE = "both" -o $TYPE = "ack" ]; then
		echo "all-ack: $ACKBASE/$o"
		echo
		echo "$ACKBASE/$o: $f"
		ackCommands $ACKBASE/$o $f
		echo
	fi
	if [ $TYPE = "both" -o $TYPE = "gnu" ]; then
		echo "all-gnu: $GNUBASE/$o"
		echo
		echo "$GNUBASE/$o: $f"
		gnuCommands $GNUBASE/$o $f
		echo
	fi
}

fi # elif of if [ -n "$SUBDIRS" ]
echo
echo "clean::"
if [ $TYPE = "both" -o $TYPE = "ack" ]; then
	echo "	rm -f $ACKBASE/$OBJDIR/*"
fi
if [ $TYPE = "both" -o $TYPE = "gnu" ]; then
	echo "	rm -f $GNUBASE/$OBJDIR/*"
fi

if [ $OBJDIR = "." ]; then
	echo
	echo "install: install-ack"
	echo
	echo "install-ack: all-ack"
	# $ARCH is from /etc/make.conf
	echo "	cp $ACKBASE/*.[ao] /usr/lib/$ARCH"
	echo
	echo "install-gnu: all-gnu"
	echo "	cp $GNUBASE/*.[ao] /usr/gnu/lib"
fi

echo
echo "include Makedepend-ack"
echo "include .depend-ack"
echo
echo "include Makedepend-gnu"
echo "include .depend-gnu"
