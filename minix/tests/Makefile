NOGCCERROR:=	yes

BINDIR?=	/usr/tests/minix-posix
FILESDIR?=	/usr/tests/minix-posix
WARNS?=		1

# Tests have no manpages
MKMAN=		no

# They are all bin-owned; by default normal executable mode
BINOWN=		bin

# Needed by testsh1.sh
FILES=		test1.c

CFLAGS+=	-fno-builtin
LDADD+=		-lm 

.include <bsd.own.mk>

SUBDIR+=		blocktest
SUBDIR+=		ddekit
SUBDIR+=		rmibtest

# Some have special flags compiling
CPPFLAGS.test56.c += -D_MINIX_SYSTEM=1

COPTS.test9.c=	-O0
COPTS.test37.c=	-O0
COPTS.test53.c=	-O0
COPTS.test68.c=	-O0

# Some have special libraries
LDADD.test59=	-lmthread
LDADD.test76=	-lutil
LDADD.test77=	-lutil

# Some have an extra file
OBJS.test57=	test57loop.o
OBJS.test56+=   common-socket.o
OBJS.test80+=   common-socket.o
OBJS.test81+=   common-socket.o

# Cache testing programs
OBJS.test71+=	testcache.o
OBJS.test72+=	testcache.o
OBJS.test74+=	testcache.o
LDADD.test72+=	-lminixfs

PROGS += testvm
OBJS.testvm+=	testcache.o
LDSTATIC.testvm= -static
LDADD.testvm+=	-lsys -ltimers 

FILES += testvm.conf

# Network stack testing programs
OBJS.test90+=	socklib.o
OBJS.test91+=	socklib.o
OBJS.test92+=	socklib.o
OBJS.test93+=	socklib.o
# Uncomment the following lines to use SOCKLIB_SWEEP_GENERATE=1/2 in socklib.c
#.PATH: ${NETBSDSRCDIR}/minix/usr.bin/trace
#OBJS.test90+=	error.o
#OBJS.test91+=	error.o
#OBJS.test92+=	error.o
#OBJS.test93+=	error.o

.if ${USE_INET6} == "no"
# Tests 91-94 will fail without IPv6 support, but they should at least compile.
CPPFLAGS.socklib.c += -DNO_INET6
CPPFLAGS.test94.c += -DNO_INET6
.endif # ${USE_INET6} == "no"

# Tests to compile, For every architecture
MINIX_TESTS= \
 1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 \
21 22 23 24 25 26 27 28 29 30 31 32 33 34 35 36 37 38 39 40 \
41 42 43 44 45 46    48 49 50    52 53 54 55 56    58 59 60 \
61       64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 \
81 82 83 84 85 86 87 88 89 90 91 92 93 94

FILES += t84_h_nonexec.sh

.if ${MACHINE_ARCH} == "i386"
MINIX_TESTS+= \
                  47          51                57 \
   62
.endif #  ${MACHINE_ARCH} == "i386"

.for t in ${MINIX_TESTS}
PROGS+= test${t}
.endfor
  
PROGS+=	t10a t11a t11b t40a t40b t40c t40d t40e t40f t40g t60a t60b \
	t67a t67b t68a t68b tvnd t84_h_spawn t84_h_spawnattr

SCRIPTS+= run check-install testinterp.sh testsh1.sh testsh2.sh testmfs.sh \
	  testisofs.sh testvnd.sh testkyua.sh testrelpol.sh testrmib.sh

# test57loop.S is not linked into the .bcl file.
# This way, we can link it in when linking the final binary
LDADD.test57+= ${${USE_BITCODE:Uno} != "no":? test57loop.o -Wl,-allow-multiple-definition:}

.if ${MKPIC} == "yes"
# Build them as dynamic executables by default if shared libraries
# are available; so that the building and executing of dynamic
# executables is tested
LDSTATIC=	-dynamic

LDFLAGS.mod+=	-shared		# make shared object

# Files which have to be compiled with -fPIC
mod.o: mod.c
	${COMPILE.c} -fPIC ${.IMPSRC}

common.o: common.c
	${COMPILE.c} -fPIC ${.IMPSRC}

# Add test that must be linked dynamically, and its dynamically loaded
# module
PROGS+=		test63 mod
.endif # ${MKPIC} == "yes"

.for o in ${PROGS}
OBJS.${o} += common.o
.endfor

.include "./arch/${MACHINE_ARCH}/Makefile.inc"

# LSC Make sure there is not leftover after a failed testrun
clean: .PHONY .MAKE
	@rm -rf DIR*

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
