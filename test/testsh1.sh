#!/bin/sh

# Shell script used to test MINIX.

# Helper function
bomb() {
	echo $*
	cd ..
	rm -rf $TESTDIR
	exit 1
}


PATH=:/bin:/usr/bin
export PATH

TESTDIR=DIR_SH1
export TESTDIR

OLDPWD=`pwd`
export OLDPWD

echo -n "Shell test  1 "
rm -rf $TESTDIR
mkdir $TESTDIR
cd $TESTDIR

f=$OLDPWD/test1.c
if test -r $f; then : ; else echo sh1 cannot read $f; exit 1; fi

#Initial setup
echo "abcdefghijklmnopqrstuvwxyz" >alpha
echo "ABCDEFGHIJKLMNOPQRSTUVWXYZ" >ALPHA
echo "0123456789" >num
echo "!@#$%^&*()_+=-{}[]:;<>?/.," >special
cp /etc/rc rc
cp /etc/passwd passwd
cat alpha ALPHA num rc passwd special >tmp
cat tmp tmp tmp >f1


#Test cp
mkdir foo
cp /etc/rc /etc/passwd foo
if cmp -s foo/rc /etc/rc ; then : ; else bomb "Error on cp test 1"; fi
if cmp -s foo/passwd /etc/passwd ; then : ; else bomb "Error on cp test 2"; fi
rm -rf foo

#Test cat
cat num num num num num >y
wc -c y >x1
echo "     55 y" >x2
if cmp -s x1 x2; then : ; else bomb "Error on cat test 1"; fi
cat <y >z
if cmp -s y z; then : ; else bomb "Error on cat test 2"; fi

#Test basename
if test `basename /usr/ast/foo.c .c` != 'foo'
   then bomb "Error on basename test 1"
fi

if test `basename a/b/c/d` != 'd'; then bomb "Error on basename test 2"; fi

#Test cdiff, sed, and patch
cp $f x.c			# x.c is a copy $f
echo "/a/s//#####/g" >s		# create sed script
sed -f s <x.c >y.c		# y.c is new version of x.c
diff -c x.c y.c >y		# y is cdiff listing
patch x.c y  2>/dev/null	# z should be y.c
if cmp -s x.c y.c; then : ; else bomb "Error in cdiff test"; fi
rm x.c* y.c s y

#Test comm, grep -v
ls /etc >x			# x = list of /etc
grep -v "passwd" x >y		# y = x except for /etc/passwd
comm -3 x y >z			# should only be 1 line, /etc/passwd
echo "passwd" >w
if cmp -s w z; then : else bomb "Error on comm test 1"; fi

comm -13 x y >z			# should be empty
if test -s z; then bomb "Error on comm test 2"; fi
rm -rf w x y z

#Test compress
compress -fc $f >x.c.Z		# compress the test file
compress -cd x.c.Z >y		# uncompress it
if cmp -s $f y; then : else bomb "Error in compress test 1"; fi
rm -rf x.c.Z y

#Test ed
cp $f x				# copy $f to x
cat >y <<END
g/a/s//#####/g
g/b/s//@@@@@/g
g/c/s//!!!!!/g
w
q
END
ed 2>/dev/null x <y >/dev/null
cat >y <<END
g/#####/s//a/g
g/@@@@@/s//b/g
g/!!!!!/s//c/g
w
q
END
ed 2>/dev/null x <y >/dev/null
if cmp -s x $f; then : ; else bomb "Error in ed test 1"; fi
rm x y

#Test expr
if test `expr 1 + 1` != 2; then bomb "Error on expr test 1"; fi
if test `expr 10000 - 1` != 9999; then bomb "Error on expr test 2"; fi
if test `expr 100 '*' 50` != 5000; then bomb "Error on expr test 3"; fi
if test `expr 120 / 5` != 24; then bomb "Error on expr test 4"; fi
if test `expr 143 % 7` != 3; then bomb "Error on expr test 5"; fi
a=100
a=`expr $a + 1`
if test $a != '101'; then bomb "Error on expr test 6"; fi

#Test fgrep
fgrep "abc" alpha >z
if cmp -s alpha z ; then : else bomb "Error on fgrep test 1"; fi
fgrep "abc" num >z
if test -s z; then bomb "Error on fgrep test 2"; fi
cat alpha num >z
fgrep "012" z >w
if cmp -s w num; then : ; else bomb "Error fgrep test 3"; fi


#Test find
date >Rabbit
echo "Rabbit" >y
find . -name Rabbit -print >z
if cmp -s y z; then : else bomb "Error on find test 1"; fi
find . -name Bunny -print >z
if test -s z; then bomb "Error on find test 2"; fi
rm Rabbit y z

#Test grep
grep "a" alpha >x
if cmp -s x alpha; then : ; else bomb "Error on grep test 1"; fi
grep "a" ALPHA >x
if test -s x; then bomb "Error on grep test 2"; fi
grep -v "0" alpha >x
if cmp -s x alpha; then : ; else bomb "Error on grep test 3"; fi
grep -s "a" XXX_nonexistent_file_XXX >x
if test -s x; then echo "Error on grep test 4"; fi
if grep -s "a" alpha >x; then : else bomb "Error on grep test 5"; fi
if grep -s "a" ALPHA >x; then bomb "Error on grep test 6"; fi

#Test head
head -1 f1 >x
if cmp -s x alpha; then : else bomb "Error on head test 1"; fi
head -2 f1 >x
cat alpha ALPHA >y
if cmp -s x y; then : else bomb "Error on head test 2"; fi

#Test ls
mkdir FOO
cp passwd FOO/z
cp alpha FOO/x
cp ALPHA FOO/y
cd FOO
ls >w
cat >w1 <<END
w
x
y
z
END
if cmp -s w w1; then : ; else bomb "Error on ls test 1"; fi
rm *
cd ..
rmdir FOO

#Test mkdir/rmdir
mkdir Foo Bar
if test -d Foo; then : ; else bomb "Error on mkdir test 1"; fi
if test -d Bar; then : ; else bomb "Error on mkdir test 2"; fi
rmdir Foo Bar
if test -d Foo; then bomb "Error on mkdir test 3"; fi
if test -d Foo; then bomb "Error on rmdir test 4"; fi

#Test mv
mkdir MVDIR
cp $f x
mv x y
mv y z
if cmp -s $f z; then : ; else bomb "Error on mv test 1"; fi
cp $f x
mv x MVDIR/y
if cmp -s $f MVDIR/y; then : ; else bomb "Error on mv test 2"; fi

#Test rev
rev <f1 | head -1 >ahpla
echo "zyxwvutsrqponmlkjihgfedcba" >x
if cmp -s x ahpla; then : ; else bomb "Error on rev test 1"; fi
rev <$f >x
rev <x >y
if cmp -s $f x; then bomb "Error on rev test 2"; fi
if cmp -s $f y; then : ; else bomb "Error on rev test 3"; fi

#Test shar
cp $f w
cp alpha x
cp ALPHA y
cp num z
shar w x y z >x1
rm w x y z
sh <x1 >/dev/null
if cmp -s w $f; then : ; else bomb "Error on shar test 1"; fi
if cmp -s x alpha; then : ; else bomb "Error on shar test 2"; fi
if cmp -s y ALPHA; then : ; else bomb "Error on shar test 3"; fi
if cmp -s z num; then : ; else bomb "Error on shar test 4"; fi

#Test sort
sort <$f >x
wc <$f >x1
wc <x >x2
if cmp -s x1 x2; then : ; else bomb "Error on sort test 1"; fi
cat >x <<END
demit 10
epitonic 40
apoop 20
bibelot 3
comate 4
END
cat >y <<END
apoop 20
bibelot 3
comate 4
demit 10
epitonic 40
END
cat >z <<END
epitonic 40
demit 10
comate 4
bibelot 3
apoop 20
END
sort <x >x1
if cmp -s y x1; then : ; else bomb "Error on sort test 2"; fi
sort -r <x1 >x2
if cmp -s x2 z; then : ; else bomb "Error on sort test 3"; fi
sort -k 2n <x |head -1 >y
echo "bibelot 3" >z
if cmp -s y z; then : ; else bomb "Error on sort test 4"; fi

#Test tail
tail -1 f1 >x
if cmp -s x special; then : ; else bomb "Error on tail test 1"; fi

#Test tsort
cat >x <<END
a d
b e
c f
a c
p z
k p
a k
a b
b c
c d
d e
e f
f k
END
cat >answer <<END
a
b
c
d
e
f
k
p
z
END
tsort <x >y
if cmp -s y answer; then : ; else bomb "Error on tsort test 1"; fi

#Test uue/uud
cp $f x
uue x
if test -s x.uue; then : ; else bomb "Error on uue/uud test 1"; fi
rm x
uud x.uue
if cmp -s x $f; then : ; else bomb "Error on uue/uud test 2"; fi

compress -fc x >x.Z 2>/dev/null
uue x.Z
rm x x.Z
uud x.uue
compress -cd x.Z >x
if cmp -s x $f; then : ; else bomb "Error on uue/uud test 3"; fi

cd ..
rm -rf $TESTDIR

echo ok
