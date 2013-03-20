BINDIR?=	/usr/tests/minix-posix
WARNS?=		1

# Tests have no manpages
MKMAN=		no

# They are all bin-owned; by default normal executable mode
BINOWN=		bin

CFLAGS+=	-fno-builtin
LDADD+=		-lm -lcompat_minix

.include <bsd.own.mk>

# Some have special flags compiling
.if ${MACHINE_ARCH} == "i386"
# LSC FIXME: Compilation error for now on ARM with that!
COPTS.test51.c=	-mhard-float
COPTS.test52.c=	-mhard-float
.endif

COPTS.test9.c=	-O0
COPTS.test37.c=	-O0
COPTS.test53.c=	-O0
COPTS.test68.c=	-O0

# Some have special libraries
LDADD.test59=	-lmthread
LDFLAGS.mod=	-shared		# make shared object

# Some have an extra file
OBJS.test57=	test57loop.o

# Cache testing programs
OBJS.test71+=	testcache.o
OBJS.test72+=	testcache.o
LDADD.test72+=	-lminixfs

PROGS += testvm
OBJS.testvm+=	testcache.o
LDADD.testvm+=	-lsys -ltimers -lminlib -static

FILES += testvm.conf

# Tests to compile, For every architecture
MINIX_TESTS= \
 1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 \
21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 \
41 42 43 44 45 46    48 49 50    52 53 54 55 56    58 59 60 \
61       64 65 66 67 68 69 70 71 72 73

.if ${MACHINE_ARCH} == "i386"
MINIX_TESTS+= \
                  47          51                57 \
   62
.endif #  ${MACHINE_ARCH} == "i386"

.if ${MACHINE_ARCH} == "earm"
# LSC Not yet supported on ARM
MKPIC:=		no
LDSTATIC:=	-static
.endif

.for t in ${MINIX_TESTS}
PROGS+= test${t}
.endfor
  
PROGS+=	t10a t11a t11b t40a t40b t40c t40d t40e t40f t60a t60b \
	t67a t67b t68a t68b

SCRIPTS+= run testinterp.sh testsh1.sh testsh2.sh

.if ${MKPIC} == "yes"
# Build them as dynamic executables by default if shared libraries
# are available; so that the building and executing of dynamic
# executables is tested
LDSTATIC=	-dynamic

# Add test that must be linked dynamically, and its dynamically loaded
# module
PROGS+=		test63 mod
.endif

.for o in $(PROGS)
OBJS.${o} += common.o
.endfor

# LSC Make sure there is not leftover after a failed testrun
clean: .PHONY .MAKE
	$(MAKE) -C select clean
	rm -rf DIR*

.include <bsd.prog.mk>
