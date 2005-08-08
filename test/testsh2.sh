#!/bin/sh

# Shell script #2 used to test MINIX.

PATH=:/bin:/usr/bin
export PATH

# CC="exec cc -wo -F"		# nonstandard flags for ACK :-(
CC=cc

ARCH=`arch`

echo -n "Shell test  2 "
rm -rf DIR_SH2
mkdir DIR_SH2			# all files are created here
cd DIR_SH2

cat >file <<END
The time has come the walrus said to talk of many things
Of shoes and ships and sealing wax of cabbages and kings
Of why the sea is boiling hot and whether pigs have wings
END
f=file				# scratch file

cat >makefile <<END		# create a makefile
all:	x.c
	@$CC x.c >/dev/null 2>&1
END
cat >x.c <<END			# create a C program
#include <stdio.h>
char s[] = {"MS-DOS: Just say no"};	/* used by strings later */
main() 
{
  int i; 
  for (i = 15; i < 18; i++) printf("%d\\n",i*i);
}
END

cat >answer <<END		# C program should produce these results
225
256
289
END

make
if test -f a.out; then : ; else echo Compilation failed; fi
a.out >x
if test -f x; then : ; else echo No compiler output; fi
if cmp -s x answer; then : ; else echo Error in cc test 1; fi

#Test chmod
echo Hi there folks >x
if test -r x; then : ; else echo Error on chmod test 1; fi
chmod 377 x
if test -r x; then test -w / || echo Error on chmod test 2; fi
chmod 700 x
if test -r x; then : ; else echo Error on chmod test 3; fi

#Test cut
cat >x <<END			# x is a test file with 3 columns
1 white bunny
2 gray  rabbits
3 brown hares
4 black conies
END

cat >answer <<END		# after cutting out cols 3-7, we get this
white
gray 
brown
black
END

cut -c 3-7 x >y			# extract columns 3-7
if cmp -s y answer; then : ; else echo Error in cut test 1; fi

#Test dd
dd if=$f of=x bs=12 count=1 2>/dev/null		# x = bytes 0-11
dd if=$f of=y bs=6 count=4 skip=2 2>/dev/null	# y = bytes 11-35
cat x y >z					# z = bytes 0-35
dd if=$f of=answer bs=9 count=4 2>/dev/null	# answer = bytes 0-35
if cmp -s z answer; then : ; else echo Error in dd test 1; fi

#Test df			# hard to make a sensible Test here
rm ?
df >x
if test -r x; then : ; else echo Error in df Test 1; fi

#Test du			# see df
rm ?
du >x
if test -r x; then : ; else echo Error in du Test 1; fi

#Test od			
head -1 $f |od >x		# see if od converts ascii to octal ok
if [ $ARCH = i86 -o $ARCH = i386 ]
then
cat >answer <<END
0000000 064124 020145 064564 062555 064040 071541 061440 066557
0000020 020145 064164 020145 060567 071154 071565 071440 064541
0000040 020144 067564 072040 066141 020153 063157 066440 067141
0000060 020171 064164 067151 071547 000012
0000071
END
else
cat >answer <<END
0000000 052150 062440 072151 066545 020150 060563 020143 067555
0000020 062440 072150 062440 073541 066162 072563 020163 060551
0000040 062040 072157 020164 060554 065440 067546 020155 060556
0000060 074440 072150 064556 063563 005000
0000071
END
fi

if cmp -s x answer; then : ; else echo Error in od test 1; fi

head -1 $f |od -d >x		# see if od converts ascii to decimal ok
if [ $ARCH = i86 -o $ARCH = i386 ]
then
cat >answer <<END
0000000 26708 08293 26996 25965 26656 29537 25376 28015
0000020 08293 26740 08293 24951 29292 29557 29472 26977
0000040 08292 28532 29728 27745 08299 26223 27936 28257
0000060 08313 26740 28265 29543 00010
0000071
END
else
cat >answer <<END
0000000 21608 25888 29801 28005 08296 24947 08291 28525
0000020 25888 29800 25888 30561 27762 30067 08307 24937
0000040 25632 29807 08308 24940 27424 28518 08301 24942
0000060 31008 29800 26990 26483 02560
0000071
END
fi

if cmp -s x answer; then : ; else echo Error in od test 2; fi

#Test paste
cat >x <<END
red
green
blue
END

cat >y <<END
rood
groen
blauw
END
cat >answer <<END
red	rood
green	groen
blue	blauw
END

paste x y >z
if cmp -s z answer; then : ; else echo Error in paste test 1; fi

#Test prep
echo >x <<END
"Hi," said Carol, laughing, "How's life?"
END

echo >answer <<END
hi
said
carol
laughing
how's
life
END

if cmp -s x answer; then : ; else echo Error in prep test 1; fi

#Test printenv
printenv >x
if grep HOME  x >/dev/null; then : ; else echo Error in printenv test 1; fi
if grep PATH  x >/dev/null; then : ; else echo Error in printenv test 2; fi
if grep SHELL x >/dev/null; then : ; else echo Error in printenv test 3; fi
if grep USER  x >/dev/null; then : ; else echo Error in printenv test 4; fi

#Test pwd
pwd >Pwd_file
cd `pwd`
pwd >x
if test -s Pwd_file;  then : ; else echo Error in pwd test 1; fi
if cmp -s Pwd_file x; then : ; else echo Error in pwd test 2; fi

#Test strings
strings a.out | grep "MS-DOS" >x
cat >answer <<END
MS-DOS: Just say no
END

if cmp -s x answer; then : ; else echo Error in strings test 1; fi

#Test sum
sum $f >x
cat >answer <<END
29904     1
END

if cmp -s x answer; then : ; else echo Error in sum test 1; fi

#Test tee
cat $f | tee x >/dev/null
if cmp -s x $f; then : ; else echo Error in tee test 1; fi

#Test true
if true ; then : ; else echo Error in true test 1; fi

#Test uniq
cat >x <<END
100
200
200
300
END

cat >answer <<END
100
200
300
END

uniq <x >y
if cmp -s y answer; then : ; else echo Error in uniq test 1; fi

#Test pipelines
cat >x <<END
the big black dog
the little white cat
the big white sheep
the little black cat
END

cat >answer <<END
   1 dog
   1 sheep
   2 big
   2 black
   2 cat
   2 little
   2 white
   4 the
END

prep x | sort | uniq -c >y1
sort +1 <y1 >y
if cmp -s y answer; then : ; else echo Error in pipeline test 1; fi

cat $f $f $f | sort | uniq >x
sort <$f >y
if cmp -s x y; then : ; else echo Error in pipeline test 2; fi

cd ..
rm -rf DIR_SH2

echo ok
